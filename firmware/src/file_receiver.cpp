// BLE file-transfer receiver implementation (protocol v3.1)
// State machine: IDLE --file_begin--> RECEIVING --file_end--> IDLE
// Chunks are written to "<path>.part" as they arrive (one ≤~505-byte
// payload per BLE write, no large RAM buffers) and flushed every 16
// chunks, at which point a file_ack is notified so the app can pace
// itself and a later resume starts from durable bytes.

#include "file_receiver.h"
#include "tile_reader.h"   // tileReader.isReady() — SD mount state
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

// Ack (and flush) every N accepted chunks
static const uint8_t ACK_EVERY_CHUNKS = 16;

// Global instance
FileReceiver fileReceiver;

void FileReceiver::begin(NotifyFn notifyFn) {
    _notify = notifyFn;
}

// ---- Notification helpers ----

template <size_t N>
static void sendDoc(FileReceiver::NotifyFn fn, StaticJsonDocument<N>& doc) {
    if (!fn) return;
    char buf[192];
    serializeJson(doc, buf, sizeof(buf));
    fn(buf);
}

void FileReceiver::emitError(const char* reason) {
    StaticJsonDocument<128> doc;
    doc["type"] = "file_error";
    doc["reason"] = reason;
    sendDoc(_notify, doc);
    Serial.printf("FileRx: error — %s\n", reason);
}

void FileReceiver::emitReady(uint32_t offset) {
    StaticJsonDocument<128> doc;
    doc["type"] = "file_ready";
    doc["offset"] = offset;
    sendDoc(_notify, doc);
    Serial.printf("FileRx: ready %s (resume offset %u)\n", _path, (unsigned)offset);
}

void FileReceiver::emitAck(uint16_t seq) {
    StaticJsonDocument<128> doc;
    doc["type"] = "file_ack";
    doc["seq"] = seq;
    sendDoc(_notify, doc);
}

void FileReceiver::emitDone() {
    StaticJsonDocument<192> doc;
    doc["type"] = "file_done";
    doc["path"] = _path;
    sendDoc(_notify, doc);
    Serial.printf("FileRx: done %s\n", _path);
}

// ---- Path helpers ----

bool FileReceiver::validTilePath(const char* path) {
    if (!path) return false;
    size_t len = strlen(path);
    if (len < 12 || len >= 128) return false;              // "/tiles/x.jpg" minimum
    if (strncmp(path, "/tiles/", 7) != 0) return false;    // tiles tree only
    if (strcmp(path + len - 4, ".jpg") != 0) return false; // JPEG tiles only
    if (strstr(path, "..") != nullptr) return false;       // no traversal
    return true;
}

bool FileReceiver::ensureParentDirs(const char* path) {
    // Walk each '/' and mkdir the prefix: /tiles, /tiles/16, /tiles/16/29876
    char prefix[128];
    size_t len = strlen(path);
    for (size_t i = 1; i < len; i++) {
        if (path[i] == '/') {
            memcpy(prefix, path, i);
            prefix[i] = '\0';
            if (!SD.exists(prefix)) {
                if (!SD.mkdir(prefix)) return false;
            }
        }
    }
    return true;
}

void FileReceiver::closeHandle() {
    if (_file) {
        _file.flush();
        _file.close();
    }
}

// ---- Control messages ----

void FileReceiver::onControlMessage(const char* json, size_t len) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, json, len)) return;

    const char* type = doc["type"];
    if (!type) return;

    if (strcmp(type, "file_begin") == 0) {
        handleBegin(doc["path"] | "", doc["size"] | 0u);
    } else if (strcmp(type, "file_end") == 0) {
        handleEnd(doc["size"] | 0u);
    } else if (strcmp(type, "file_abort") == 0) {
        handleAbort();
    }
    // Unknown control types are ignored
}

void FileReceiver::handleBegin(const char* path, uint32_t size) {
    if (!validTilePath(path)) {
        emitError("bad_path");
        return;
    }
    if (!tileReader.isReady()) {
        emitError("no_sd");
        return;
    }

    // A new begin supersedes any transfer in progress (its .part stays
    // on the card and can still be resumed by a later begin).
    if (_state == State::RECEIVING) closeHandle();
    _state = State::IDLE;

    if (!ensureParentDirs(path)) {
        emitError("mkdir_failed");
        return;
    }

    strlcpy(_path, path, sizeof(_path));
    snprintf(_partPath, sizeof(_partPath), "%s.part", _path);

    // Append mode: creates the .part if absent, resumes if present.
    _file = SD.open(_partPath, FILE_APPEND);
    if (!_file) {
        emitError("open_failed");
        return;
    }

    uint32_t offset = _file.size();
    if (offset > size) {
        // Stale .part larger than the announced file — it cannot belong
        // to this transfer; restart from zero so file_end can match.
        _file.close();
        SD.remove(_partPath);
        _file = SD.open(_partPath, FILE_APPEND);
        if (!_file) {
            emitError("open_failed");
            return;
        }
        offset = 0;
    }

    _expectedSeq = 0;
    _lastAckSeq = 0;
    _hasAck = false;
    _chunksSinceAck = 0;
    _state = State::RECEIVING;

    emitReady(offset);
}

void FileReceiver::handleEnd(uint32_t size) {
    if (_state != State::RECEIVING) return;

    _file.flush();
    uint32_t actual = _file.size();
    _file.close();
    _state = State::IDLE;

    if (actual != size) {
        // Keep the .part — a later file_begin resumes from its size.
        emitError("size_mismatch");
        return;
    }

    if (SD.exists(_path)) SD.remove(_path);
    if (SD.rename(_partPath, _path)) {
        emitDone();
    } else {
        emitError("rename_failed");
    }
}

void FileReceiver::handleAbort() {
    if (_state != State::RECEIVING) return;
    closeHandle();
    _state = State::IDLE;
    SD.remove(_partPath);
    Serial.printf("FileRx: aborted %s (.part deleted)\n", _path);
}

// ---- Data chunks ----

void FileReceiver::onDataChunk(const uint8_t* data, size_t len) {
    if (_state != State::RECEIVING || !_file) return;
    if (!data || len < 4) return;

    uint16_t seq = (uint16_t)(data[0] | (data[1] << 8));
    uint16_t declared = (uint16_t)(data[2] | (data[3] << 8));
    size_t avail = len - 4;
    size_t payloadLen = (declared <= avail) ? declared : avail;

    if (seq != _expectedSeq) {
        // Out-of-order / duplicate: drop the payload and remind the app
        // where the contiguous stream actually stands.
        if (_hasAck) emitAck(_lastAckSeq);
        return;
    }

    if (payloadLen > 0) {
        _file.write(data + 4, payloadLen);
    }
    _lastAckSeq = seq;
    _hasAck = true;
    _expectedSeq = (uint16_t)(seq + 1);

    if (++_chunksSinceAck >= ACK_EVERY_CHUNKS) {
        _chunksSinceAck = 0;
        _file.flush();   // acked bytes are durable — safe resume point
        emitAck(_lastAckSeq);
    }
}

void FileReceiver::onDisconnect() {
    if (_state != State::RECEIVING) return;
    closeHandle();       // .part stays on the card for resume
    _state = State::IDLE;
    Serial.println("FileRx: disconnect mid-transfer — .part kept for resume");
}
