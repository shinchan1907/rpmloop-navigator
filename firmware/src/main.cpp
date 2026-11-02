// RPMLOOP Navigator — main.cpp
//
// Hardware: ESP32-S3 Mini, ST7789V 320x240 landscape, HR2046 touch, SD card (SPI2_HOST)
// Software: LovyanGFX, LVGL 8.3.11, BLE server (protocol v3.0.0), TileReader

#include <Arduino.h>
#include "lvgl_port.h"
#include "tile_reader.h"
#include "ble_server.h"
#include "ui_screens.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("========================================");
    Serial.println("   RPMLOOP Navigator v3.0 Booting...   ");
    Serial.println("========================================");

    // Initialize PSRAM
    if (psramFound()) {
        Serial.printf("PSRAM detected: %lu bytes free\n", (unsigned long)ESP.getFreePsram());
    } else {
        Serial.println("WARNING: No PSRAM detected!");
    }

    // 1. Initialize SD Card & TileReader (SPI2_HOST, CS=GPIO13)
    // Hardware bus must be initialized before display to avoid register contention
    if (tileReader.init()) {
        Serial.println("[1/4] SD Card & TileReader initialized successfully");
    } else {
        Serial.println("[1/4] TileReader init: SD card not detected (maps will show placeholder)");
    }

    // 2. Initialize display + LVGL (shared SPI2_HOST with bus_shared=true)
    lvgl_port_init();
    Serial.println("[2/4] Display & LVGL initialized");

    // 3. Initialize BLE Server & start advertising as "RPMLOOP"
    rpmBle.init();
    Serial.println("[3/4] BLE server initialized, advertising as \"RPMLOOP\"");

    // 4. Initialize UI screens & state
    ui_init();
    Serial.println("[4/4] UI initialized");

    // Memory status
    Serial.printf("Boot complete. Free heap: %lu bytes, Free PSRAM: %lu bytes\n",
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());
}

void loop() {
    // Service display rendering & touch input
    lvgl_port_update();

    // Service BLE protocol & notifications
    rpmBle.update();

    // Service UI state updates (~50 Hz)
    static uint32_t last_ui_update = 0;
    uint32_t now = millis();
    if (now - last_ui_update >= 20) {
        last_ui_update = now;
        ui_update();
    }

    // Process serial diagnostic / screenshot commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "CMD:DUMP_BMP") {
            lvgl_port_dump_bmp(Serial);
        } else if (cmd == "CMD:NAV:SPLASH") {
            ui_navigate(Screen::SPLASH);
        } else if (cmd == "CMD:NAV:HOME0") {
            ui_navigate(Screen::HOME);
            ui_home_set_page(0);
        } else if (cmd == "CMD:NAV:HOME1") {
            ui_navigate(Screen::HOME);
            ui_home_set_page(1);
        } else if (cmd == "CMD:NAV:HOME2") {
            ui_navigate(Screen::HOME);
            ui_home_set_page(2);
        } else if (cmd == "CMD:NAV:SETTINGS") {
            ui_navigate(Screen::SETTINGS);
        } else if (cmd == "CMD:NAV:DOCS") {
            ui_navigate(Screen::DOCS);
        }
    }

    delay(5);  // Yield to RTOS and maintain responsive tick rate
}
