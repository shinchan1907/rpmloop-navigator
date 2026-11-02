// BLE server for RPMLOOP Navigator
// Advertises as "RPMLOOP", handles all protocol characteristics

#pragma once

#include "ble_data.h"

class RpmloopBLE {
public:
    // Start BLE server and begin advertising
    void init();

    // Check connection state
    BleConnState getState() const;

    // Send a media command to the connected app
    void sendMediaCommand(MediaCmd cmd);

    // Send brightness level acknowledgement
    void sendBrightnessAck(uint8_t level);

    // Process any pending BLE data (call from loop)
    void update();

    // Is a client connected and handshake complete?
    bool isReady() const;

private:
    void setupService();
    void startAdvertising();
};

// Global BLE instance
extern RpmloopBLE rpmBle;
