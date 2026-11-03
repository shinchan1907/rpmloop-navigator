// BLE file-transfer receiver (protocol v3.1) for RPMLOOP Navigator
// Lets the companion app write map tiles onto the SD card over BLE:
//   FILE_CTRL (...b1): JSON control messages in, JSON replies out via notify
//   FILE_DATA (...b0): binary chunks [seq u16 LE][len u16 LE][payload]
//
// Incoming bytes land in a "<path>.part" sibling file first and are
// renamed into place only when the final size matches — a half-written
// tile never appears at its final path, and an interrupted transfer can
// resume from the .part size reported in file_ready.

#pragma once

#include <cstdint>
#include <cstddef>
#include <SD.h>

class FileReceiver {
public:
    // Sink the BLE layer provides for FILE_CTRL notifications.
    typedef void (*NotifyFn)(const char* json);

    // Register the notify sink. Call once during BLE setup.
    void begin(NotifyFn notifyFn);

    // FILE_CTRL write received (JSON text, not NUL-terminated).
    void onControlMessage(const char* json, size_t len);

    // FILE_DATA write received (binary chunk with 4-byte header).
    void onDataChunk(const uint8_t* data, size_t len);

    // BLE link dropped mid-transfer: close the file handle safely and
    // KEEP the .part file so the transfer can resume on reconnect.
    void onDisconnect();

    bool isReceiving() const { return _state == State::RECEIVING; }

private:
    enum class State : uint8_t { IDLE, RECEIVING };

    // Control message handlers (doc already parsed by onControlMessage)
    void handleBegin(const char* path, uint32_t size);
    void handleEnd(uint32_t size);
    void handleAbort();

    // Close the open .part handle (bytes stay on the card).
    void closeHandle();

    // Validate a target tile path: /tiles/....jpg, no "..".
    static bool validTilePath(const char* path);

    // Create every parent directory of path (e.g. /tiles/16/29876).
    static bool ensureParentDirs(const char* path);

    // Notification builders
    void emitError(const char* reason);
    void emitReady(uint32_t offset);
    void emitAck(uint16_t seq);
    void emitDone();

    NotifyFn _notify = nullptr;
    State    _state = State::IDLE;

    File     _file;                    // open .part handle (RECEIVING only)
    char     _path[128];               // final path, e.g. /tiles/16/29876/16981.jpg
    char     _partPath[sizeof(_path) + 8]; // _path + ".part"
    uint16_t _expectedSeq = 0;         // next in-order chunk seq this session
    uint16_t _lastAckSeq = 0;          // last contiguous seq received
    bool     _hasAck = false;          // any chunk accepted this session
    uint8_t  _chunksSinceAck = 0;      // accepted chunks since last file_ack
};

// Global file receiver instance
extern FileReceiver fileReceiver;
