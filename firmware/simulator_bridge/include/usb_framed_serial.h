#ifndef USB_FRAMED_SERIAL_H
#define USB_FRAMED_SERIAL_H

#include <Arduino.h>
#include <ArduinoJson.h>

class UsbFramedSerial {
public:
    static void init(unsigned long baud = 115200) {
        Serial.begin(baud);
    }

    static void emitLog(const char* level, const char* msg) {
        JsonDocument doc;
        doc["level"] = level;
        doc["msg"] = msg;
        doc["ms"] = millis();
        Serial.print("[RPMLOPP_LOG] ");
        serializeJson(doc, Serial);
        Serial.println();
    }

    static void emitAck(uint32_t seq, const char* status, const char* type = "ACK") {
        JsonDocument doc;
        doc["seq"] = seq;
        doc["status"] = status;
        doc["type"] = type;
        doc["ms"] = millis();
        Serial.print("[RPMLOPP_ACK] ");
        serializeJson(doc, Serial);
        Serial.println();
    }

    static void emitErr(const char* code, const char* msg, uint32_t seq = 0) {
        JsonDocument doc;
        doc["code"] = code;
        doc["msg"] = msg;
        if (seq > 0) doc["seq"] = seq;
        doc["ms"] = millis();
        Serial.print("[RPMLOPP_ERR] ");
        serializeJson(doc, Serial);
        Serial.println();
    }

    static void emitEvent(const char* eventName, JsonDocument& data) {
        JsonDocument wrapper;
        wrapper["event"] = eventName;
        wrapper["data"] = data;
        wrapper["ms"] = millis();
        Serial.print("[RPMLOPP_EVT] ");
        serializeJson(wrapper, Serial);
        Serial.println();
    }
};

#endif // USB_FRAMED_SERIAL_H
