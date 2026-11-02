// SD Card tile reader implementation for RPMLOOP Navigator
// Reads JPEG tiles from FAT32 SD card, decodes to RGB565 in PSRAM

#include "tile_reader.h"
#include "lgfx_config.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "ff.h"

// JPEG decoder — LovyanGFX bundled TJpgDec
#include <lgfx/utility/lgfx_tjpgd.h>
#include "lv_conf.h"

// SD card CS pin & dedicated SPIClass for SPI2_HOST
// Hardware: SCK=GPIO6, MISO=GPIO8, MOSI=GPIO7, CS=GPIO13 (shared with ST7789 display)
static const int SD_CS_PIN = 13;
static SPIClass sd_spi(FSPI);

// Global instance
TileReader tileReader;

// Extern display for SPI bus sharing
extern LGFX tft;

// ---- JPEG decode context ----
struct JpegDecodeCtx {
    const uint8_t* src;
    size_t         src_len;
    size_t         src_pos;
    uint16_t*      dst;
    int            dst_width;
    int            dst_height;
};

// TJpgDec stream input callback
// When buf != nullptr: copy up to len bytes from src into buf
// When buf == nullptr: skip len bytes in stream
static uint32_t tjpgd_input_cb(void* dev, uint8_t* buf, uint32_t len) {
    JpegDecodeCtx* ctx = (JpegDecodeCtx*)dev;
    if (!ctx) return 0;
    if (ctx->src_pos >= ctx->src_len) return 0;

    size_t remain = ctx->src_len - ctx->src_pos;
    size_t to_read = (len < remain) ? len : remain;

    if (buf) {
        memcpy(buf, ctx->src + ctx->src_pos, to_read);
    }
    ctx->src_pos += to_read;
    return (uint32_t)to_read;
}

// TJpgDec output callback — writes RGB565 pixels honoring LV_COLOR_16_SWAP
static uint32_t tjpgd_output_cb(void* dev, void* bitmap, JRECT* rect) {
    JpegDecodeCtx* ctx = (JpegDecodeCtx*)dev;
    const uint8_t* src = (const uint8_t*)bitmap;

    int w = rect->right - rect->left + 1;
    int h = rect->bottom - rect->top + 1;

    for (int y = 0; y < h; y++) {
        int dst_y = rect->top + y;
        if (dst_y >= ctx->dst_height) break;
        for (int x = 0; x < w; x++) {
            int dst_x = rect->left + x;
            if (dst_x < ctx->dst_width) {
                uint8_t r = *src++;
                uint8_t g = *src++;
                uint8_t b = *src++;

                // RGB565 with byte-swap honoring LV_COLOR_16_SWAP=1
#if LV_COLOR_16_SWAP
                uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                ctx->dst[dst_y * ctx->dst_width + dst_x] = (c >> 8) | (c << 8);
#else
                uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                ctx->dst[dst_y * ctx->dst_width + dst_x] = c;
#endif
            } else {
                src += 3;
            }
        }
    }
    return 1;
}

bool TileReader::init() {
    // Initialize cache slots
    for (int i = 0; i < MAX_CACHED_TILES; i++) {
        _cache[i].valid = false;
        _cache[i].pixels = nullptr;
        _cache[i].last_used = 0;
    }

    Serial.println("\n[SD] Initializing SD Card (SCK=6, MISO=8, MOSI=7, CS=13)...");

    // Ensure display CS (GPIO9) and SD CS (GPIO13) are deselected HIGH
    pinMode(9, OUTPUT);
    digitalWrite(9, HIGH);
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);

    // Hardware Bus Recovery: Clock SCK with CS HIGH to release MISO (DO)
    pinMode(7, OUTPUT);
    digitalWrite(7, HIGH); // MOSI HIGH
    pinMode(6, OUTPUT);
    pinMode(8, INPUT_PULLUP);
    gpio_set_pull_mode(GPIO_NUM_8, GPIO_PULLUP_ONLY);

    for (int i = 0; i < 200; i++) {
        digitalWrite(6, LOW);
        delayMicroseconds(2);
        digitalWrite(6, HIGH);
        delayMicroseconds(2);
    }

    // Initialize dedicated SPIClass for SD card on SPI2_HOST
    sd_spi.begin(6, 8, 7, SD_CS_PIN);
    gpio_set_pull_mode(GPIO_NUM_8, GPIO_PULLUP_ONLY);

    // Send at least 80 clocks with CS HIGH to enter SPI mode cleanly
    digitalWrite(SD_CS_PIN, HIGH);
    sd_spi.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
    for (int i = 0; i < 20; i++) sd_spi.transfer(0xFF);
    sd_spi.endTransaction();
    gpio_set_pull_mode(GPIO_NUM_8, GPIO_PULLUP_ONLY);
    Serial.printf("[SD] Pre-mount MISO pin level: %d (expected 1)\n", digitalRead(8));

    // Attempt mount at 4MHz first
    if (!SD.begin(SD_CS_PIN, sd_spi, 4000000, "/sd", 5, false)) {
        Serial.println("[SD] 4MHz mount failed, trying 400kHz fallback...");
        SD.end();
        delay(50);
        digitalWrite(SD_CS_PIN, HIGH);
        for (int i = 0; i < 20; i++) sd_spi.transfer(0xFF);
        if (!SD.begin(SD_CS_PIN, sd_spi, 400000, "/sd", 5, false)) {
            Serial.println("[SD] SD.begin failed at 400kHz as well.");
            _mounted = false;
            return false;
        }
    }

    uint8_t ct = SD.cardType();
    if (ct == CARD_NONE) {
        Serial.println("[SD] Card type reported NONE.");
        _mounted = false;
        return false;
    }

    Serial.printf("[SD] Card detected! Type: %s, Size: %llu MB (~%llu GB)\n",
        ct == CARD_MMC ? "MMC" : ct == CARD_SD ? "SD" : ct == CARD_SDHC ? "SDHC" : "Unknown",
        SD.cardSize() / (1024 * 1024), (SD.cardSize() / (1024 * 1024) + 512) / 1024);

    _mounted = true;
    return true;
}

bool TileReader::isReady() const {
    return _mounted;
}

void TileReader::buildPath(char* buf, size_t len, int32_t x, int32_t y, uint8_t z) {
    snprintf(buf, len, "/tiles/%u/%ld/%ld.jpg", (unsigned)z, (long)x, (long)y);
}

bool TileReader::decodeJpeg(const uint8_t* data, size_t len, uint16_t* outPixels) {
    if (!data || len == 0 || !outPixels) return false;

    JpegDecodeCtx ctx;
    ctx.src = data;
    ctx.src_len = len;
    ctx.src_pos = 0;
    ctx.dst = outPixels;
    ctx.dst_width = TILE_SIZE;
    ctx.dst_height = TILE_SIZE;

    static constexpr size_t TJPGD_POOL_SIZE = 4096;
    uint8_t* pool = (uint8_t*)malloc(TJPGD_POOL_SIZE);
    if (!pool) pool = (uint8_t*)ps_malloc(TJPGD_POOL_SIZE);
    if (!pool) return false;

    lgfxJdec jd;
    JRESULT res = lgfx_jd_prepare(&jd, tjpgd_input_cb, pool, TJPGD_POOL_SIZE, &ctx);
    if (res == JDR_OK) {
        res = lgfx_jd_decomp(&jd, tjpgd_output_cb, 0);
    }

    free(pool);
    return (res == JDR_OK);
}

CachedTile* TileReader::findInCache(int32_t x, int32_t y, uint8_t z) {
    for (int i = 0; i < MAX_CACHED_TILES; i++) {
        if (_cache[i].valid && _cache[i].x == x && _cache[i].y == y && _cache[i].z == z) {
            _cache[i].last_used = millis();
            return &_cache[i];
        }
    }
    return nullptr;
}

CachedTile* TileReader::evictLRU() {
    // First look for an empty slot
    for (int i = 0; i < MAX_CACHED_TILES; i++) {
        if (!_cache[i].valid) {
            // Allocate pixel buffer in PSRAM
            if (!_cache[i].pixels) {
                _cache[i].pixels = (uint16_t*)ps_malloc(TILE_SIZE * TILE_SIZE * sizeof(uint16_t));
                if (!_cache[i].pixels) {
                    Serial.println("PSRAM alloc failed for tile!");
                    return nullptr;
                }
            }
            return &_cache[i];
        }
    }

    // Evict LRU
    int lru_idx = 0;
    uint32_t oldest = UINT32_MAX;
    for (int i = 0; i < MAX_CACHED_TILES; i++) {
        if (_cache[i].last_used < oldest) {
            oldest = _cache[i].last_used;
            lru_idx = i;
        }
    }

    _cache[lru_idx].valid = false;
    return &_cache[lru_idx];
}

bool TileReader::loadTile(int32_t x, int32_t y, uint8_t z, CachedTile* slot) {
    char path[64];
    buildPath(path, sizeof(path), x, y, z);

    tft.waitDisplay(); // Ensure display isn't using SPI

    File f = SD.open(path, FILE_READ);
    if (!f) {
        return false;
    }

    size_t fsize = f.size();
    if (fsize == 0 || fsize > 200000) { // Sanity: tiles shouldn't exceed ~200KB
        f.close();
        return false;
    }

    // Read JPEG data into temporary buffer
    uint8_t* jpegBuf = (uint8_t*)ps_malloc(fsize);
    if (!jpegBuf) {
        f.close();
        return false;
    }

    size_t bytesRead = f.read(jpegBuf, fsize);
    f.close();

    if (bytesRead != fsize) {
        free(jpegBuf);
        return false;
    }

    // Decode JPEG to RGB565
    bool ok = decodeJpeg(jpegBuf, fsize, slot->pixels);
    free(jpegBuf);

    if (ok) {
        slot->x = x;
        slot->y = y;
        slot->z = z;
        slot->valid = true;
        slot->last_used = millis();
    }

    return ok;
}

bool TileReader::scaleFallback(int32_t x, int32_t y, uint8_t z, CachedTile* slot) {
    // Try lower zoom levels until we find a tile
    for (uint8_t fz = z - 1; fz >= 12; fz--) {
        int shift = z - fz;
        int32_t fx = x >> shift;
        int32_t fy = y >> shift;

        // Check if this lower-zoom tile exists (try to load it)
        // First check cache
        CachedTile* cached = findInCache(fx, fy, fz);
        if (cached) {
            // Scale up the relevant quadrant
            int srcSize = TILE_SIZE >> shift;
            int srcOffX = (x - (fx << shift)) * srcSize;
            int srcOffY = (y - (fy << shift)) * srcSize;

            // Nearest-neighbour upscale
            for (int dy = 0; dy < TILE_SIZE; dy++) {
                for (int dx = 0; dx < TILE_SIZE; dx++) {
                    int sx = srcOffX + dx * srcSize / TILE_SIZE;
                    int sy = srcOffY + dy * srcSize / TILE_SIZE;
                    if (sx >= TILE_SIZE) sx = TILE_SIZE - 1;
                    if (sy >= TILE_SIZE) sy = TILE_SIZE - 1;
                    slot->pixels[dy * TILE_SIZE + dx] = cached->pixels[sy * TILE_SIZE + sx];
                }
            }

            slot->x = x;
            slot->y = y;
            slot->z = z;
            slot->valid = true;
            slot->last_used = millis();
            return true;
        }

        // Try loading from SD
        char path[64];
        buildPath(path, sizeof(path), fx, fy, fz);
        tft.waitDisplay();
        if (SD.exists(path)) {
            // Load the lower-zoom tile into a temporary cache slot, then scale
            // For simplicity, try to get it through the normal path
            CachedTile* tempSlot = evictLRU();
            if (tempSlot && loadTile(fx, fy, fz, tempSlot)) {
                // Now scale from tempSlot to our target slot
                int srcSize = TILE_SIZE >> shift;
                int srcOffX = (x - (fx << shift)) * srcSize;
                int srcOffY = (y - (fy << shift)) * srcSize;

                for (int dy = 0; dy < TILE_SIZE; dy++) {
                    for (int dx = 0; dx < TILE_SIZE; dx++) {
                        int sx = srcOffX + dx * srcSize / TILE_SIZE;
                        int sy = srcOffY + dy * srcSize / TILE_SIZE;
                        if (sx >= TILE_SIZE) sx = TILE_SIZE - 1;
                        if (sy >= TILE_SIZE) sy = TILE_SIZE - 1;
                        slot->pixels[dy * TILE_SIZE + dx] = tempSlot->pixels[sy * TILE_SIZE + sx];
                    }
                }
                slot->x = x;
                slot->y = y;
                slot->z = z;
                slot->valid = true;
                slot->last_used = millis();
                return true;
            }
        }
    }
    return false;
}

const CachedTile* TileReader::getTile(int32_t x, int32_t y, uint8_t z) {
    if (!_mounted) return nullptr;

    // Check cache first
    CachedTile* cached = findInCache(x, y, z);
    if (cached) return cached;

    // Get a cache slot (may evict LRU)
    CachedTile* slot = evictLRU();
    if (!slot) return nullptr;

    // Try loading the exact tile
    if (loadTile(x, y, z, slot)) {
        return slot;
    }

    // Fallback: try lower zoom levels
    if (scaleFallback(x, y, z, slot)) {
        return slot;
    }

    // No tile available at all
    return nullptr;
}

void TileReader::prefetchNeighbours(int32_t cx, int32_t cy, uint8_t z) {
    // Prefetch a 3x3 grid around center (if not already cached)
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue; // center already loaded
            int32_t tx = cx + dx;
            int32_t ty = cy + dy;
            if (!findInCache(tx, ty, z)) {
                CachedTile* slot = evictLRU();
                if (slot) {
                    loadTile(tx, ty, z, slot);
                }
            }
        }
    }
}

void TileReader::clearCache() {
    for (int i = 0; i < MAX_CACHED_TILES; i++) {
        _cache[i].valid = false;
    }
}

uint64_t TileReader::cardSizeBytes() const {
    if (!_mounted) return 0;
    return SD.cardSize();
}

const char* TileReader::cardType() const {
    if (!_mounted) return "None";
    uint8_t ct = SD.cardType();
    switch (ct) {
        case CARD_MMC:  return "MMC";
        case CARD_SD:   return "SD";
        case CARD_SDHC: return "SDHC";
        default:        return "Unknown";
    }
}

bool TileReader::decodeJpegBuffer(const uint8_t* jpegBuf, size_t fsize, uint16_t* outPixels, int targetW, int targetH) {
    if (!jpegBuf || fsize == 0 || !outPixels) return false;

    JpegDecodeCtx ctx;
    ctx.src = jpegBuf;
    ctx.src_len = fsize;
    ctx.src_pos = 0;
    ctx.dst = outPixels;
    ctx.dst_width = targetW;
    ctx.dst_height = targetH;

    // Clear target buffer first (black)
    memset(outPixels, 0, targetW * targetH * sizeof(uint16_t));

    static constexpr size_t TJPGD_POOL_SIZE = 4096;
    uint8_t* pool = (uint8_t*)malloc(TJPGD_POOL_SIZE);
    if (!pool) pool = (uint8_t*)ps_malloc(TJPGD_POOL_SIZE);
    if (!pool) return false;

    lgfxJdec jd;
    JRESULT res = lgfx_jd_prepare(&jd, tjpgd_input_cb, pool, TJPGD_POOL_SIZE, &ctx);
    if (res == JDR_OK) {
        uint8_t scale = 0;
        if (jd.width > targetW * 4 || jd.height > targetH * 4) scale = 3;
        else if (jd.width > targetW * 2 || jd.height > targetH * 2) scale = 2;
        else if (jd.width > targetW || jd.height > targetH) scale = 1;

        res = lgfx_jd_decomp(&jd, tjpgd_output_cb, scale);
    }

    free(pool);
    return (res == JDR_OK);
}

bool TileReader::loadDocJpeg(const char* docName, uint16_t* outPixels, int targetW, int targetH) {
    if (!docName || !outPixels) return false;

    // 1. Try reading from SD card if mounted
    if (_mounted) {
        char path[64];
        snprintf(path, sizeof(path), "/docs/%s.jpg", docName);
        tft.waitDisplay();

        if (!SD.exists(path)) {
            snprintf(path, sizeof(path), "/docs/%s.jpeg", docName);
        }

        if (SD.exists(path)) {
            File f = SD.open(path, FILE_READ);
            if (f) {
                size_t fsize = f.size();
                if (fsize > 0 && fsize <= 600000) {
                    uint8_t* jpegBuf = (uint8_t*)ps_malloc(fsize);
                    if (jpegBuf) {
                        size_t bytesRead = f.read(jpegBuf, fsize);
                        f.close();
                        if (bytesRead == fsize) {
                            bool ok = decodeJpegBuffer(jpegBuf, fsize, outPixels, targetW, targetH);
                            free(jpegBuf);
                            if (ok) return true;
                        } else {
                            free(jpegBuf);
                        }
                    } else {
                        f.close();
                    }
                } else {
                    f.close();
                }
            }
        }
    }

    // SD-only by design: no documents are embedded in the
    // firmware binary. Docs live on the card under /docs/.
    return false;
}
