// BLE server implementation for RPMLOOP Navigator
// Advertises as "RPMLOOP", implements protocol v3.0.0

#include "ble_server.h"
#include "ble_data.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

// UUIDs from protocol_spec.json
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_HANDSHAKE_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_POSITION_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define CHAR_NAV_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define CHAR_POLYLINE_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define CHAR_MEDIA_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26ac"
#define CHAR_MEDIA_ART_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ad"
#define CHAR_TRACKING_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26ae"
#define CHAR_DEVICE_CMD_UUID "beb5483e-36e1-4688-b7f5-ea07361b26af"

// Global instance
RpmloopBLE rpmBle;

// Internal state
static BLEServer* pServer = nullptr;
static BLECharacteristic* pHandshake = nullptr;
static BLECharacteristic* pPosition = nullptr;
static BLECharacteristic* pNavState = nullptr;
static BLECharacteristic* pPolyline = nullptr;
static BLECharacteristic* pMedia = nullptr;
static BLECharacteristic* pMediaArt = nullptr;
static BLECharacteristic* pTracking = nullptr;
static BLECharacteristic* pDeviceCmd = nullptr;

static bool _connected = false;
static bool _handshakeOk = false;

// Album art chunked receive buffer
static uint8_t* _artBuf = nullptr;
static uint32_t _artBufLen = 0;
static uint32_t _artExpectedLen = 0;

// ---- Server callbacks ----
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* s) override {
        _connected = true;
        _handshakeOk = false;
        deviceState.ble = BleConnState::CONNECTED;
        Serial.println("BLE: Client connected");
    }

    void onDisconnect(BLEServer* s) override {
        _connected = false;
        _handshakeOk = false;
        deviceState.ble = BleConnState::DISCONNECTED;
        Serial.println("BLE: Client disconnected");
        // Restart advertising
        delay(500);
        BLEDevice::startAdvertising();
        deviceState.ble = BleConnState::ADVERTISING;
    }
};

// ---- Parse position update ----
static void parsePosition(const char* json, size_t len) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, json, len)) return;

    const char* type = doc["type"];
    if (!type || strcmp(type, "pos") != 0) return;

    deviceState.gps.lat = doc["lat"] | 0.0;
    deviceState.gps.lon = doc["lon"] | 0.0;
    deviceState.gps.speed_kmh = doc["speed_kmh"] | 0.0f;
    deviceState.gps.heading = doc["heading"] | 0.0f;
    deviceState.gps.accuracy_m = doc["accuracy_m"] | 0.0f;
    deviceState.gps.timestamp = doc["ts"] | 0u;
    deviceState.gps.valid = true;
}

// ---- Parse nav state ----
static void parseNavState(const char* json, size_t len) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, json, len)) return;

    const char* type = doc["type"];
    if (!type || strcmp(type, "nav") != 0) return;

    deviceState.nav.active = doc["active"] | false;
    deviceState.nav.maneuver = parseManeuver(doc["maneuver"] | "");
    deviceState.nav.distance_m = doc["distance_m"] | 0u;
    strlcpy(deviceState.nav.street, doc["street"] | "", MAX_STREET_LEN);
    strlcpy(deviceState.nav.next_street, doc["next_street"] | "", MAX_STREET_LEN);
    deviceState.nav.eta_min = doc["eta_min"] | 0;
    deviceState.nav.remaining_km = doc["remaining_km"] | 0.0f;
}

// ---- Parse polyline chunk ----
static void parsePolyline(const char* json, size_t len) {
    StaticJsonDocument<2048> doc;
    if (deserializeJson(doc, json, len)) return;

    const char* type = doc["type"];
    if (!type || strcmp(type, "poly") != 0) return;

    uint16_t seq = doc["seq"] | 0;
    uint16_t total = doc["total"] | 1;

    if (seq == 0) {
        // New route — reset
        strlcpy(deviceState.route.route_id, doc["route_id"] | "", 8);
        deviceState.route.count = 0;
        deviceState.route.complete = false;
    }

    JsonArray points = doc["points"];
    for (JsonArray pt : points) {
        if (deviceState.route.count < MAX_POLYLINE_PTS) {
            deviceState.route.points[deviceState.route.count].lat = pt[0];
            deviceState.route.points[deviceState.route.count].lon = pt[1];
            deviceState.route.count++;
        }
    }

    if (seq + 1 >= total) {
        deviceState.route.complete = true;
    }
}

// ---- Parse media state ----
static void parseMedia(const char* json, size_t len) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, json, len)) return;

    const char* type = doc["type"];
    if (!type || strcmp(type, "media") != 0) return;

    deviceState.media.playing = doc["playing"] | false;
    strlcpy(deviceState.media.title, doc["title"] | "", MAX_TITLE_LEN);
    strlcpy(deviceState.media.artist, doc["artist"] | "", MAX_ARTIST_LEN);
    strlcpy(deviceState.media.album, doc["album"] | "", MAX_ALBUM_LEN);
    deviceState.media.duration_s = doc["duration_s"] | 0;
    deviceState.media.position_s = doc["position_s"] | 0;
    deviceState.media.has_art = doc["has_art"] | false;
}

// ---- Parse tracking state ----
static void parseTracking(const char* json, size_t len) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, json, len)) return;

    const char* type = doc["type"];
    if (!type || strcmp(type, "track") != 0) return;

    deviceState.tracking.active = doc["active"] | false;
    deviceState.tracking.distance_km = doc["distance_km"] | 0.0f;
    deviceState.tracking.elapsed_s = doc["elapsed_s"] | 0u;
    deviceState.tracking.avg_speed_kmh = doc["avg_speed_kmh"] | 0.0f;
    deviceState.tracking.max_speed_kmh = doc["max_speed_kmh"] | 0.0f;
    deviceState.tracking.current_speed_kmh = doc["current_speed_kmh"] | 0.0f;
}

// ---- Characteristic callbacks ----
class HandshakeCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        String val = c->getValue();
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, val.c_str(), val.length())) return;

        const char* type = doc["type"];
        if (type && strcmp(type, "hello") == 0) {
            Serial.printf("BLE: Handshake from %s v%s\n",
                (const char*)(doc["app_name"] | "unknown"),
                (const char*)(doc["version"] | "?"));

            // Send hello_ack
            StaticJsonDocument<256> ack;
            ack["type"] = "hello_ack";
            ack["version"] = "3.0.0";
            ack["device_name"] = "RPMLOOP";
            ack["fw_version"] = "3.0.0";
            ack["has_sd"] = true; // Will be updated at runtime
            JsonObject disp = ack.createNestedObject("display");
            disp["width"] = 320;
            disp["height"] = 240;

            char buf[256];
            serializeJson(ack, buf, sizeof(buf));
            c->setValue(buf);
            c->notify();

            _handshakeOk = true;
            deviceState.ble = BleConnState::HANDSHAKE_OK;
            Serial.println("BLE: Handshake OK");
        }
    }
};

class PositionCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        String val = c->getValue();
        parsePosition(val.c_str(), val.length());
    }
};

class NavStateCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        String val = c->getValue();
        parseNavState(val.c_str(), val.length());
    }
};

class PolylineCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        String val = c->getValue();
        parsePolyline(val.c_str(), val.length());
    }
};

class MediaCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        String val = c->getValue();
        parseMedia(val.c_str(), val.length());
    }
};

class MediaArtCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        // Binary album art data — chunked transfer
        const uint8_t* data = c->getData();
        size_t len = c->getLength();

        if (len < 4) return;

        // First 4 bytes: total expected length (little-endian)
        // If this is a new transfer (artBufLen == 0), read the header
        if (_artBufLen == 0) {
            _artExpectedLen = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
            if (_artExpectedLen > 120 * 120 * 3) { // Sanity limit
                _artExpectedLen = 0;
                return;
            }
            // Allocate in PSRAM
            if (_artBuf) free(_artBuf);
            _artBuf = (uint8_t*)ps_malloc(_artExpectedLen);
            if (!_artBuf) {
                _artExpectedLen = 0;
                return;
            }
            data += 4;
            len -= 4;
        }

        // Append data
        size_t remaining = _artExpectedLen - _artBufLen;
        size_t copy = (len < remaining) ? len : remaining;
        memcpy(_artBuf + _artBufLen, data, copy);
        _artBufLen += copy;

        // Check if transfer complete
        if (_artBufLen >= _artExpectedLen) {
            // Store in device state
            if (deviceState.art.jpeg_data) free(deviceState.art.jpeg_data);
            deviceState.art.jpeg_data = _artBuf;
            deviceState.art.jpeg_len = _artBufLen;
            deviceState.art.decoded = false;
            _artBuf = nullptr;
            _artBufLen = 0;
            _artExpectedLen = 0;
            Serial.printf("BLE: Album art received (%u bytes)\n", (unsigned int)deviceState.art.jpeg_len);
        }
    }
};

class TrackingCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        String val = c->getValue();
        parseTracking(val.c_str(), val.length());
    }
};

// ---- RpmloopBLE implementation ----

void RpmloopBLE::setupService() {
    BLEService* pService = pServer->createService(BLEUUID(SERVICE_UUID), 30); // 30 handles

    // Handshake (read/write/notify)
    pHandshake = pService->createCharacteristic(
        CHAR_HANDSHAKE_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pHandshake->setCallbacks(new HandshakeCallback());

    // Position (write)
    pPosition = pService->createCharacteristic(CHAR_POSITION_UUID, BLECharacteristic::PROPERTY_WRITE);
    pPosition->setCallbacks(new PositionCallback());

    // Nav state (write)
    pNavState = pService->createCharacteristic(CHAR_NAV_UUID, BLECharacteristic::PROPERTY_WRITE);
    pNavState->setCallbacks(new NavStateCallback());

    // Polyline (write)
    pPolyline = pService->createCharacteristic(CHAR_POLYLINE_UUID, BLECharacteristic::PROPERTY_WRITE);
    pPolyline->setCallbacks(new PolylineCallback());

    // Media state (write)
    pMedia = pService->createCharacteristic(CHAR_MEDIA_UUID, BLECharacteristic::PROPERTY_WRITE);
    pMedia->setCallbacks(new MediaCallback());

    // Media art (write, binary)
    pMediaArt = pService->createCharacteristic(CHAR_MEDIA_ART_UUID, BLECharacteristic::PROPERTY_WRITE);
    pMediaArt->setCallbacks(new MediaArtCallback());

    // Tracking (write)
    pTracking = pService->createCharacteristic(CHAR_TRACKING_UUID, BLECharacteristic::PROPERTY_WRITE);
    pTracking->setCallbacks(new TrackingCallback());

    // Device commands (read/notify — device -> app)
    pDeviceCmd = pService->createCharacteristic(
        CHAR_DEVICE_CMD_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );

    pService->start();
}

void RpmloopBLE::startAdvertising() {
    BLEAdvertising* pAdv = BLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->setScanResponse(true);
    pAdv->setMinPreferred(0x06);
    pAdv->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    deviceState.ble = BleConnState::ADVERTISING;
    Serial.println("BLE: Advertising as RPMLOOP");
}

void RpmloopBLE::init() {
    BLEDevice::init("RPMLOOP");
    BLEDevice::setMTU(512);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    setupService();
    startAdvertising();

    deviceState.ble = BleConnState::ADVERTISING;
    Serial.println("BLE: Initialized");
}

BleConnState RpmloopBLE::getState() const {
    return deviceState.ble;
}

void RpmloopBLE::sendMediaCommand(MediaCmd cmd) {
    if (!_connected || !_handshakeOk || !pDeviceCmd) return;

    StaticJsonDocument<64> doc;
    doc["type"] = "media_cmd";
    switch (cmd) {
        case MediaCmd::PLAY:     doc["action"] = "play"; break;
        case MediaCmd::PAUSE:    doc["action"] = "pause"; break;
        case MediaCmd::NEXT:     doc["action"] = "next"; break;
        case MediaCmd::PREVIOUS: doc["action"] = "previous"; break;
        default: return;
    }

    char buf[64];
    serializeJson(doc, buf, sizeof(buf));
    pDeviceCmd->setValue(buf);
    pDeviceCmd->notify();
}

void RpmloopBLE::sendBrightnessAck(uint8_t level) {
    if (!_connected || !_handshakeOk || !pDeviceCmd) return;

    StaticJsonDocument<64> doc;
    doc["type"] = "brightness";
    doc["level"] = level;

    char buf[64];
    serializeJson(doc, buf, sizeof(buf));
    pDeviceCmd->setValue(buf);
    pDeviceCmd->notify();
}

void RpmloopBLE::update() {
    // Currently no periodic work needed — all handling is callback-driven
}

bool RpmloopBLE::isReady() const {
    return _connected && _handshakeOk;
}
