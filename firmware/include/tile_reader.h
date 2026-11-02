// SD Card tile reader for RPMLOOP Navigator
// Reads map tiles from FAT32 SD card in tiles/{z}/{x}/{y}.jpg layout
// Decodes JPEG to RGB565 and caches in PSRAM

#pragma once

#include <cstdint>
#include <cstddef>

// Tile dimensions (standard web mercator tiles)
static const int TILE_SIZE = 256;

// A decoded tile cached in PSRAM
struct CachedTile {
    int32_t  x;        // tile X coordinate
    int32_t  y;        // tile Y coordinate
    uint8_t  z;        // zoom level
    bool     valid;    // whether this tile has valid data
    uint16_t* pixels;  // RGB565 pixel data (256*256 = 65536 pixels, 128KB)
    uint32_t last_used; // millis() timestamp for LRU eviction
};

class TileReader {
public:
    // Initialize the SD card on SPI2_HOST with CS=GPIO13
    bool init();

    // Format the SD card as FAT32

    // Check if SD card is mounted and readable
    bool isReady() const;

    // Get a decoded tile. Returns pointer to cached tile, or nullptr if unavailable.
    // Automatically handles PSRAM caching and LRU eviction.
    // If exact tile is missing, attempts to find lower-zoom fallback.
    const CachedTile* getTile(int32_t x, int32_t y, uint8_t z);

    // Prefetch tiles around the current viewport (call from background)
    void prefetchNeighbours(int32_t center_x, int32_t center_y, uint8_t z);

    // Free all cached tiles
    void clearCache();

    // Decode an in-memory JPEG buffer to 16-bit RGB565 target (TJpgDec)
    bool decodeJpegBuffer(const uint8_t* jpegBuf, size_t fsize, uint16_t* outPixels, int targetW, int targetH);

    // Load and decode a document JPEG from /docs/<name>.jpg or built-in flash fallback
    bool loadDocJpeg(const char* docName, uint16_t* outPixels, int targetW, int targetH);

    // Get SD card info
    uint64_t cardSizeBytes() const;
    const char* cardType() const;

private:
    bool _mounted = false;

    // PSRAM tile cache (max tiles limited by available PSRAM)
    static const int MAX_CACHED_TILES = 12;  // 12 * 128KB = ~1.5MB PSRAM
    CachedTile _cache[MAX_CACHED_TILES];

    // Find tile in cache
    CachedTile* findInCache(int32_t x, int32_t y, uint8_t z);

    // Evict least-recently-used tile and return its slot
    CachedTile* evictLRU();

    // Read and decode a tile JPEG from SD card
    bool loadTile(int32_t x, int32_t y, uint8_t z, CachedTile* slot);

    // Build file path: /tiles/{z}/{x}/{y}.jpg
    void buildPath(char* buf, size_t len, int32_t x, int32_t y, uint8_t z);

    // Decode JPEG buffer to RGB565 pixels in PSRAM
    bool decodeJpeg(const uint8_t* data, size_t len, uint16_t* outPixels);

    // Scale a lower-zoom tile to fill a higher-zoom slot (fallback)
    bool scaleFallback(int32_t x, int32_t y, uint8_t z, CachedTile* slot);
};

// Global tile reader instance
extern TileReader tileReader;
