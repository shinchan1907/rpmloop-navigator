#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

#include "pinout_config.h"
#include "protocol_def.h"
#include "display_driver.h"

// Hardware Display Driver Instance (320x240 Native SPI LCD)
DisplayDriver display(RPMLOPP_VIRTUAL_LCD_W, RPMLOPP_VIRTUAL_LCD_H);

// BLE Server & Characteristics
static BLEServer* pServer = nullptr;
static BLECharacteristic* pCmdNavChar = nullptr;
static BLECharacteristic* pStatusAckChar = nullptr;
static BLECharacteristic* pDeviceInfoChar = nullptr;

static bool deviceConnected = false;
static uint32_t expectedSeq = 1;
static char activeSessionId[32] = {0};

// Helper: RGB WS2812 Control
void setStatusLed(uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_BUILTIN
    rgbLedWriteOrdered(RGB_BUILTIN, LED_COLOR_ORDER_GRB, r, g, b);
#endif
}

// BLE Server Connection Callbacks
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        deviceConnected = true;
        setStatusLed(0, 0, 64); // SOLID BLUE when connected
    }

    void onDisconnect(BLEServer* pServer) override {
        deviceConnected = false;
        activeSessionId[0] = '\0';
        expectedSeq = 1;
        setStatusLed(64, 0, 0); // SOLID RED when disconnected
        display.drawStandbyScreen();
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
        if (err) return;

        int version = doc["v"] | 0;
        if (version != RPMLOPP_PROTOCOL_VERSION) return;

        const char* type = doc["type"] | "";
        uint32_t seq = doc["seq"] | 0;
        const char* sid = doc["sid"] | "";

        // Handshake
        if (strcmp(type, "HELLO") == 0) {
            strncpy(activeSessionId, sid, sizeof(activeSessionId) - 1);
            expectedSeq = seq + 1;
            return;
        }

        // Backlight Brightness Control
        if (strcmp(type, "SET_BRIGHTNESS") == 0) {
            int brightness = doc["payload"]["brightness"] | 80;
            display.setBrightness((uint8_t)brightness);
            return;
        }

        // Navigation Packet Handling
        if (strcmp(type, "NAV_START") == 0 || strcmp(type, "NAV_UPDATE") == 0 ||
            strcmp(type, "NAV_PAUSE") == 0 || strcmp(type, "NAV_RESUME") == 0 ||
            strcmp(type, "NAV_STOP") == 0) {

            JsonObject payload = doc["payload"];
            if (payload.isNull()) return;

            NavigationState state;
            memset(&state, 0, sizeof(NavigationState));
            state.valid = true;
            state.seq = seq;
            state.last_update_ms = millis();
            strncpy(state.session_id, sid, sizeof(state.session_id) - 1);

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
            
            const char* mStr = payload["maneuver"] | "STRAIGHT";
            state.maneuver = parseManeuver(mStr);
            
            uint32_t dist = 0;
            if (payload["dist_m"].is<uint32_t>()) {
                dist = payload["dist_m"].as<uint32_t>();
            } else if (payload["distance_m"].is<uint32_t>()) {
                dist = payload["distance_m"].as<uint32_t>();
            }
            state.dist_to_turn_m = dist;

            const char* rd = payload["road"] | (payload["road_name"] | "Road Ahead");
            strncpy(state.road, rd, sizeof(state.road) - 1);

            const char* ins = payload["instr"] | (payload["instruction"] | rd);
            strncpy(state.instruction, ins, sizeof(state.instruction) - 1);

            state.rem_dist_m = payload["rem_dist_m"] | 0;
            state.rem_time_s = payload["rem_time_s"] | 0;
            state.progress_pct = payload["progress_pct"] | 0;
            state.off_route = payload["off_route"] | false;
            state.gps_acc_m = payload["gps_acc_m"] | 0.0f;

            // Render live state directly on physical SPI LCD
            display.renderNavScreen(state);
        }
    }
};

void setup() {
    Serial.begin(115200);

    // Initialize physical SPI LCD & Backlight
    display.init();

    // Initialize BLE Server
    BLEDevice::init(RPMLOPP_BLE_DEVICE_NAME);
    BLEDevice::setMTU(256);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService* pService = pServer->createService(RPMLOPP_SERVICE_UUID);

    pCmdNavChar = pService->createCharacteristic(
        RPMLOPP_CHAR_CMD_NAV_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pCmdNavChar->setCallbacks(new CmdNavCallbacks());

    pStatusAckChar = pService->createCharacteristic(
        RPMLOPP_CHAR_STATUS_ACK_UUID,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    pStatusAckChar->addDescriptor(new BLE2902());

    pDeviceInfoChar = pService->createCharacteristic(
        RPMLOPP_CHAR_DEVICE_INFO_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    JsonDocument infoDoc;
    infoDoc["model"] = RPMLOPP_HARDWARE_MODEL;
    infoDoc["fw_ver"] = "2.0.0-PROD";
    infoDoc["proto_ver"] = RPMLOPP_PROTOCOL_VERSION;
    infoDoc["lcd_w"] = RPMLOPP_VIRTUAL_LCD_W;
    infoDoc["lcd_h"] = RPMLOPP_VIRTUAL_LCD_H;
    String infoStr;
    serializeJson(infoDoc, infoStr);
    pDeviceInfoChar->setValue(infoStr.c_str());

    pService->start();

    // Advertising setup
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);

    BLEAdvertisementData advData;
    advData.setFlags(0x06);
    advData.setName(RPMLOPP_BLE_DEVICE_NAME);
    pAdvertising->setAdvertisementData(advData);

    BLEAdvertisementData scanRespData;
    scanRespData.setName(RPMLOPP_BLE_DEVICE_NAME);
    scanRespData.setCompleteServices(BLEUUID(RPMLOPP_SERVICE_UUID));
    pAdvertising->setScanResponseData(scanRespData);

    BLEDevice::startAdvertising();

    // Initial Standby LED
    setStatusLed(64, 0, 0); // SOLID RED on boot
}

void loop() {
    // Assert active WS2812 LED color
    if (deviceConnected) {
        setStatusLed(0, 0, 64); // SOLID BLUE
    } else {
        setStatusLed(64, 0, 0); // SOLID RED
    }
    delay(200);
}
