#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

#include "pinout_config.h"
#include "protocol_def.h"
#include "bike_state.h"
#include "mock_provider.h"
#include "lvgl_port.h"
#include "ui/screens.h"

// ==============================================================================
// RPMLOOP production firmware 2.1.0
//   BLE peripheral (protocol v1, JSON)  +  LVGL UI on the physical panel.
// Data flow: phone packets -> NavigationState -> bike_state_apply_nav() ->
// BikeState -> LVGL screens. With no phone, MockProvider animates BikeState
// so the device demos standalone. Display rendering lives entirely in the UI
// layer (the old DisplayDriver stub never drove a single pixel — removed).
// ==============================================================================

static BLEServer* pServer = nullptr;
static BLECharacteristic* pStatusAckChar = nullptr;

static bool deviceConnected = false;
static char activeSessionId[32] = {0};
static bool navScreenShownForSession = false;

// RGB status LED (Waveshare board only; harmless no-op on the bench S3 Mini).
static int s_ledR = -1, s_ledG = -1, s_ledB = -1;
static void setStatusLed(uint8_t r, uint8_t g, uint8_t b) {
    if (r == s_ledR && g == s_ledG && b == s_ledB) return;   // write on change only
    s_ledR = r; s_ledG = g; s_ledB = b;
#ifdef RGB_BUILTIN
    rgbLedWriteOrdered(RGB_BUILTIN, LED_COLOR_ORDER_GRB, r, g, b);
#endif
}

// ACKs / errors back to the phone. The pre-2.1.0 firmware created this
// characteristic but NEVER sent anything on it — the iOS app measures latency
// from ACKs and the protocol spec requires them, so this is a real fix.
static void sendBleAck(uint32_t seq, const char* status, const char* type) {
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
}

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* srv) override {
        (void)srv;
        deviceConnected = true;
        bike_state_on_ble_connect();
        setStatusLed(0, 0, 64); // BLUE = connected
    }

    void onDisconnect(BLEServer* srv) override {
        deviceConnected = false;
        activeSessionId[0] = '\0';
        navScreenShownForSession = false;
        bike_state_on_ble_disconnect();
        setStatusLed(64, 0, 0); // RED = disconnected
        srv->startAdvertising();
    }
};

class CmdNavCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override {
        String rxValue = pCharacteristic->getValue();
        if (rxValue.length() == 0) return;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, rxValue.c_str());
        if (err) {
            sendBleAck(0, "REJECTED_MALFORMED", "ERROR");
            return;
        }

        // NOTE on "crc": the spec fixtures carry a crc field but no algorithm
        // is defined anywhere in the repo (fixtures use dummy values 1234 /
        // 5432; the iOS app hashes a different string than the Python test).
        // Until the spec pins one algorithm, crc is accepted and ignored —
        // rejecting on it would break both real companion apps.
        int version = doc["v"] | 0;
        if (version != RPMLOPP_PROTOCOL_VERSION) {
            sendBleAck(doc["seq"] | 0, "REJECTED_VERSION", "ERROR");
            return;
        }

        const char* type = doc["type"] | "";
        uint32_t seq = doc["seq"] | 0;
        const char* sid = doc["sid"] | "";

        if (strcmp(type, "HELLO") == 0) {
            strncpy(activeSessionId, sid, sizeof(activeSessionId) - 1);
            activeSessionId[sizeof(activeSessionId) - 1] = '\0';
            navScreenShownForSession = false;
            sendBleAck(seq, "ACCEPTED", "HELLO_ACK");
            return;
        }

        if (strcmp(type, "PING") == 0) {
            sendBleAck(seq, "ACCEPTED", "PONG");
            return;
        }

        // Display brightness: the bench panel's backlight is wired to 3V3
        // (no PWM pin), so brightness drives the UI's software dim overlay.
        if (strcmp(type, "SET_BRIGHTNESS") == 0) {
            int brightness = doc["payload"]["brightness"] | 80;
            bike_control_set_brightness(brightness);
            sendBleAck(seq, "ACCEPTED", "ACK");
            return;
        }

        if (strcmp(type, "NAV_START") == 0 || strcmp(type, "NAV_UPDATE") == 0 ||
            strcmp(type, "NAV_PAUSE") == 0 || strcmp(type, "NAV_RESUME") == 0 ||
            strcmp(type, "NAV_STOP") == 0) {

            JsonObject payload = doc["payload"];
            if (payload.isNull()) {
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

            // Senders disagree on units/keys (iOS: spd_mps; Android WebView:
            // speed_kmh + spd_mps; GMaps mirror: speed_kmh only) — accept all.
            float spd = 0.0f;
            if (!payload["spd_mps"].isNull()) {
                spd = payload["spd_mps"].as<float>();
            } else if (!payload["speed_kmh"].isNull()) {
                spd = payload["speed_kmh"].as<float>() / 3.6f;
            }
            state.speed_mps = spd;
            state.heading_deg = payload["head_deg"] | 0.0f;
            state.maneuver = parseManeuver(payload["maneuver"] | "NONE");

            uint32_t dist = 0;
            if (!payload["dist_m"].isNull()) {
                dist = payload["dist_m"].as<uint32_t>();
            } else if (!payload["distance_m"].isNull()) {
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

            // Feed the UI through the BikeState seam (screens read it on
            // their 250 ms refresh; no direct rendering from this callback).
            bike_state_apply_nav(state);

            // First live packet of a session: jump straight to the nav screen.
            if (!navScreenShownForSession && state.status == NAV_STATUS_NAVIGATING) {
                navScreenShownForSession = true;
                ui_show(UI_NAV);
            }
            sendBleAck(seq, "ACCEPTED", "ACK");
            return;
        }

        sendBleAck(seq, "REJECTED_UNSUPPORTED", "ERROR");
    }
};

void setup() {
    Serial.begin(115200);

    bike_state_init();
    lvgl_port_init();          // display + LVGL + input devices
    ui_init();                 // screens (boots into Boot -> Home)
    mock_provider_init();      // standalone demo data until the phone talks

    // BLE peripheral (protocol v1).
    BLEDevice::init(RPMLOPP_BLE_DEVICE_NAME);
    BLEDevice::setMTU(256);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService* pService = pServer->createService(RPMLOPP_SERVICE_UUID);

    BLECharacteristic* pCmdNavChar = pService->createCharacteristic(
        RPMLOPP_CHAR_CMD_NAV_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pCmdNavChar->setCallbacks(new CmdNavCallbacks());

    pStatusAckChar = pService->createCharacteristic(
        RPMLOPP_CHAR_STATUS_ACK_UUID,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    pStatusAckChar->addDescriptor(new BLE2902());

    BLECharacteristic* pDeviceInfoChar = pService->createCharacteristic(
        RPMLOPP_CHAR_DEVICE_INFO_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    JsonDocument infoDoc;
    infoDoc["model"] = RPMLOPP_HARDWARE_MODEL;
    infoDoc["fw_ver"] = RPMLOPP_FIRMWARE_VERSION;   // was hardcoded "2.0.0-PROD" vs header "1.0.0"
    infoDoc["proto_ver"] = RPMLOPP_PROTOCOL_VERSION;
    infoDoc["lcd_w"] = RPMLOPP_VIRTUAL_LCD_W;
    infoDoc["lcd_h"] = RPMLOPP_VIRTUAL_LCD_H;
    String infoStr;
    serializeJson(infoDoc, infoStr);
    pDeviceInfoChar->setValue(infoStr.c_str());

    pService->start();

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
    setStatusLed(64, 0, 0); // RED = waiting for phone
}

void loop() {
    uint32_t now = millis();

    mock_provider_update(now);       // demo data (yields nav fields to live BLE)
    bike_state_check_stale(now);     // live feed silent > 5 s -> STALE pill
    if (lvgl_port_home_pressed() && ui_current() != UI_HOME && ui_current() != UI_BOOT) {
        ui_show(UI_HOME);
    }
    if (lvgl_port_play_pressed()) {
        ui_on_play_button();
    }
    lv_timer_handler();
    delay(5);
}
