#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

#include "protocol_def.h"
#include "usb_framed_serial.h"
#include "virtual_display.h"

// Globals
static BLEServer* pServer = nullptr;
static BLECharacteristic* pCmdNavChar = nullptr;
static BLECharacteristic* pStatusAckChar = nullptr;
static BLECharacteristic* pDeviceInfoChar = nullptr;

static bool deviceConnected = false;
static bool oldDeviceConnected = false;
static VirtualDisplay virtualDisplay(RPMLOPP_VIRTUAL_LCD_W, RPMLOPP_VIRTUAL_LCD_H);

static uint32_t expectedSeq = 1;
static char activeSessionId[32] = {0};
static uint32_t lastHeartbeatMs = 0;

// Onboard RGB Status LED for Waveshare ESP32-S3-Zero
// WS2812B uses GRB hardware color ordering:
// RED  = Powered On, Awaiting Connection (not connected to phone)
// BLUE = Successfully Connected to Phone
static void setStatusLed(uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_BUILTIN
    rgbLedWriteOrdered(RGB_BUILTIN, LED_COLOR_ORDER_GRB, r, g, b);
#else
    neopixelWrite(21, r, g, b);
#endif
}

// Helper to map string to maneuver enum
static ManeuverType parseManeuver(const char* m) {
    if (!m) return MANEUVER_NONE;
    if (strcmp(m, "STRAIGHT") == 0) return MANEUVER_STRAIGHT;
    if (strcmp(m, "SLIGHT_RIGHT") == 0) return MANEUVER_SLIGHT_RIGHT;
    if (strcmp(m, "TURN_RIGHT") == 0) return MANEUVER_TURN_RIGHT;
    if (strcmp(m, "SHARP_RIGHT") == 0) return MANEUVER_SHARP_RIGHT;
    if (strcmp(m, "UTURN") == 0) return MANEUVER_UTURN;
    if (strcmp(m, "SHARP_LEFT") == 0) return MANEUVER_SHARP_LEFT;
    if (strcmp(m, "TURN_LEFT") == 0) return MANEUVER_TURN_LEFT;
    if (strcmp(m, "SLIGHT_LEFT") == 0) return MANEUVER_SLIGHT_LEFT;
    if (strcmp(m, "ROUNDABOUT") == 0) return MANEUVER_ROUNDABOUT;
    if (strcmp(m, "DESTINATION") == 0) return MANEUVER_DESTINATION;
    return MANEUVER_NONE;
}

// Send BLE Notification back to phone
static void sendBleAck(uint32_t seq, const char* status, const char* type = "ACK") {
    if (!deviceConnected || !pStatusAckChar) return;

    JsonDocument doc;
    doc["v"] = RPMLOPP_PROTOCOL_VERSION;
    doc["type"] = type;
    doc["seq"] = seq;
    doc["status"] = status;
    doc["t_ms"] = millis();

    String output;
    serializeJson(doc, output);
    pStatusAckChar->setValue((uint8_t*)output.c_str(), output.length());
    pStatusAckChar->notify();

    UsbFramedSerial::emitAck(seq, status, type);
}

// BLE Server Callbacks
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        deviceConnected = true;
        virtualDisplay.setConnectionStatus(NAV_STATUS_CONNECTED_IDLE);
        UsbFramedSerial::emitLog("INFO", "BLE Client Connected");
        setStatusLed(0, 0, 64); // Turn onboard RGB LED BLUE on mobile connect
    }

    void onDisconnect(BLEServer* pServer) override {
        deviceConnected = false;
        virtualDisplay.setConnectionStatus(NAV_STATUS_DISCONNECTED);
        activeSessionId[0] = '\0';
        expectedSeq = 1;
        UsbFramedSerial::emitLog("WARN", "BLE Client Disconnected");
        setStatusLed(64, 0, 0); // Turn onboard RGB LED RED on mobile disconnect
        pServer->startAdvertising();
    }
};

// BLE Characteristic Callbacks for CMD_NAV
class CmdNavCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override {
        String rxValue = pCharacteristic->getValue();
        if (rxValue.length() == 0) return;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, rxValue.c_str());
        if (err) {
            UsbFramedSerial::emitErr("ERR_MALFORMED_JSON", err.c_str());
            return;
        }

        int version = doc["v"] | 0;
        if (version != RPMLOPP_PROTOCOL_VERSION) {
            UsbFramedSerial::emitErr("ERR_VERSION_MISMATCH", "Unsupported protocol version", doc["seq"] | 0);
            sendBleAck(doc["seq"] | 0, "REJECTED_VERSION", "ERROR");
            return;
        }

        const char* type = doc["type"] | "";
        uint32_t seq = doc["seq"] | 0;
        const char* sid = doc["sid"] | "";

        // HELLO Handshake
        if (strcmp(type, "HELLO") == 0) {
            strncpy(activeSessionId, sid, sizeof(activeSessionId) - 1);
            expectedSeq = seq + 1;
            sendBleAck(seq, "ACCEPTED", "HELLO_ACK");
            UsbFramedSerial::emitLog("INFO", "Handshake HELLO accepted");
            return;
        }

        // PING Handshake
        if (strcmp(type, "PING") == 0) {
            sendBleAck(seq, "ACCEPTED", "PONG");
            return;
        }

        // BRIGHTNESS CONTROL
        if (strcmp(type, "SET_BRIGHTNESS") == 0) {
            int brightness = doc["payload"]["brightness"] | 100;
            if (brightness < 0) brightness = 0;
            if (brightness > 100) brightness = 100;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "LCD Brightness set to %d%%", brightness);
            UsbFramedSerial::emitLog("INFO", logMsg);
            sendBleAck(seq, "ACCEPTED", "ACK");
            return;
        }

        // NAVIGATION COMMANDS
        if (strcmp(type, "NAV_START") == 0 || strcmp(type, "NAV_UPDATE") == 0 ||
            strcmp(type, "NAV_PAUSE") == 0 || strcmp(type, "NAV_RESUME") == 0 ||
            strcmp(type, "NAV_STOP") == 0) {

            JsonObject payload = doc["payload"];
            if (payload.isNull()) {
                UsbFramedSerial::emitErr("ERR_MISSING_PAYLOAD", "Nav command missing payload object", seq);
                sendBleAck(seq, "REJECTED_MALFORMED", "ERROR");
                return;
            }

            NavigationState state;
            memset(&state, 0, sizeof(NavigationState));
            state.valid = true;
            state.seq = seq;
            state.last_update_ms = millis();
            strncpy(state.session_id, sid, sizeof(state.session_id) - 1);
            const char* rid = doc["rid"] | "";
            strncpy(state.route_id, rid, sizeof(state.route_id) - 1);

            if (strcmp(type, "NAV_STOP") == 0) {
                state.status = NAV_STATUS_CONNECTED_IDLE;
            } else if (strcmp(type, "NAV_PAUSE") == 0) {
                state.status = NAV_STATUS_PAUSED;
            } else {
                state.status = NAV_STATUS_NAVIGATING;
            }

            state.is_sim = payload["is_sim"] | false;
            state.lat = payload["lat"] | 0.0;
            state.lon = payload["lon"] | 0.0;
            
            float spd = 0.0f;
            if (payload["spd_mps"].is<float>()) {
                spd = payload["spd_mps"].as<float>();
            } else if (payload["speed_kmh"].is<float>()) {
                spd = payload["speed_kmh"].as<float>() / 3.6f;
            }
            state.speed_mps = spd;
            state.heading_deg = payload["head_deg"] | 0.0f;
            state.maneuver = parseManeuver(payload["maneuver"] | "NONE");
            
            uint32_t dist = 0;
            if (payload["dist_m"].is<uint32_t>()) {
                dist = payload["dist_m"].as<uint32_t>();
            } else if (payload["distance_m"].is<uint32_t>()) {
                dist = payload["distance_m"].as<uint32_t>();
            }
            state.dist_to_turn_m = dist;

            const char* rd = payload["road"] | (payload["road_name"] | "");
            strncpy(state.road, rd, sizeof(state.road) - 1);

            const char* ins = payload["instr"] | (payload["instruction"] | rd);
            strncpy(state.instruction, ins, sizeof(state.instruction) - 1);

            state.rem_dist_m = payload["rem_dist_m"] | 0;
            state.rem_time_s = payload["rem_time_s"] | 0;
            state.progress_pct = payload["progress_pct"] | 0;
            state.off_route = payload["off_route"] | false;
            state.gps_acc_m = payload["gps_acc_m"] | 0.0f;

            // Update display
            virtualDisplay.updateNavState(state);

            // Acknowledge receipt
            sendBleAck(seq, "ACCEPTED", "ACK");
            return;
        }

        UsbFramedSerial::emitErr("ERR_UNKNOWN_TYPE", type, seq);
        sendBleAck(seq, "REJECTED_UNSUPPORTED", "ERROR");
    }
};

void setup() {
    UsbFramedSerial::init(115200);
    delay(1000); // Allow USB CDC to enumerate

    UsbFramedSerial::emitLog("INFO", "RPMLopp Nav Firmware v1.0.0 booting...");

    // Initialize BLE
    BLEDevice::init(RPMLOPP_BLE_DEVICE_NAME);
    BLEDevice::setMTU(256);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService* pService = pServer->createService(RPMLOPP_SERVICE_UUID);

    // Command/Nav Characteristic
    pCmdNavChar = pService->createCharacteristic(
        RPMLOPP_CHAR_CMD_NAV_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pCmdNavChar->setCallbacks(new CmdNavCallbacks());

    // Status/Ack Characteristic
    pStatusAckChar = pService->createCharacteristic(
        RPMLOPP_CHAR_STATUS_ACK_UUID,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    pStatusAckChar->addDescriptor(new BLE2902());

    // Device Info Characteristic
    pDeviceInfoChar = pService->createCharacteristic(
        RPMLOPP_CHAR_DEVICE_INFO_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    JsonDocument infoDoc;
    infoDoc["model"] = RPMLOPP_HARDWARE_MODEL;
    infoDoc["fw_ver"] = RPMLOPP_FIRMWARE_VERSION;
    infoDoc["proto_ver"] = RPMLOPP_PROTOCOL_VERSION;
    infoDoc["lcd_w"] = RPMLOPP_VIRTUAL_LCD_W;
    infoDoc["lcd_h"] = RPMLOPP_VIRTUAL_LCD_H;
    String infoStr;
    serializeJson(infoDoc, infoStr);
    pDeviceInfoChar->setValue(infoStr.c_str());

    pService->start();

    // Setup Advertising (Split cleanly between AdvData and ScanResponse for 100% iOS compatibility)
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // 7.5ms min interval for fast iOS discovery
    pAdvertising->setMaxPreferred(0x12); // 22.5ms max interval for iOS

    // Primary Advertisement Packet (Flags + Short Complete Local Name: 12 bytes total, well within 31B)
    BLEAdvertisementData advData;
    advData.setFlags(0x06); // General Discoverable + BR/EDR not supported
    advData.setName(RPMLOPP_BLE_DEVICE_NAME);
    pAdvertising->setAdvertisementData(advData);

    // Scan Response Packet (128-bit Custom Service UUID + Local Name)
    BLEAdvertisementData scanRespData;
    scanRespData.setName(RPMLOPP_BLE_DEVICE_NAME);
    scanRespData.setCompleteServices(BLEUUID(RPMLOPP_SERVICE_UUID));
    pAdvertising->setScanResponseData(scanRespData);

    BLEDevice::startAdvertising();

    UsbFramedSerial::emitLog("INFO", "BLE advertising started: " RPMLOPP_BLE_DEVICE_NAME);
    virtualDisplay.setConnectionStatus(NAV_STATUS_DISCONNECTED);
    setStatusLed(64, 0, 0); // Turn LED RED on boot (waiting for mobile)
}

void loop() {
    uint32_t now = millis();

    // Check stale navigation timeout (>5000ms)
    virtualDisplay.checkStaleTimeout(now);

    // Re-start advertising if client disconnected
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // Give bluetooth stack chance to reset
        pServer->startAdvertising();
        UsbFramedSerial::emitLog("INFO", "Restarted BLE advertising");
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    // Continuously enforce LED color matching live BLE hardware state
    if (deviceConnected) {
        setStatusLed(0, 0, 64); // Solid Blue when connected
    } else {
        setStatusLed(64, 0, 0); // Solid Red when NOT connected
    }

    // Process optional serial commands from desktop bridge
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "PING") {
            Serial.println("[RPMLOPP_ACK] {\"status\":\"PONG\"}");
        } else if (cmd == "STATUS") {
            virtualDisplay.publishState();
        } else if (cmd == "DISCONNECT") {
            deviceConnected = false;
            oldDeviceConnected = false;
            virtualDisplay.setConnectionStatus(NAV_STATUS_DISCONNECTED);
            setStatusLed(64, 0, 0);
            pServer->startAdvertising();
            Serial.println("[RPMLOPP_ACK] {\"status\":\"FORCE_DISCONNECTED\"}");
        }
    }

    // Periodic heartbeat every 10s
    if (now - lastHeartbeatMs > 10000) {
        lastHeartbeatMs = now;
        JsonDocument stats;
        stats["free_heap"] = ESP.getFreeHeap();
        stats["ble_connected"] = deviceConnected;
        stats["uptime_s"] = now / 1000;
        UsbFramedSerial::emitEvent("HEARTBEAT", stats);
    }

    delay(20);
}
