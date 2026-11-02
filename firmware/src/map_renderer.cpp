// Map renderer implementation
// Renders Web Mercator tiles from SD card onto an LVGL canvas

#include "map_renderer.h"
#include "lgfx_config.h"
#include <Arduino.h>
#include <cmath>

// Display dimensions
static const int DISP_W = 320;
static const int DISP_H = 240;

MapRenderer mapRenderer;

// ---- Web Mercator math ----
namespace MapMath {
    static const double MAP_PI = 3.14159265358979323846;

    int32_t lonToTileX(double lon, uint8_t zoom) {
        return (int32_t)floor((lon + 180.0) / 360.0 * (1 << zoom));
    }

    int32_t latToTileY(double lat, uint8_t zoom) {
        double latRad = lat * MAP_PI / 180.0;
        return (int32_t)floor((1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / MAP_PI) / 2.0 * (1 << zoom));
    }

    int32_t lonToPixelX(double lon, uint8_t zoom) {
        double x = (lon + 180.0) / 360.0 * (1 << zoom) * TILE_SIZE;
        return (int32_t)x;
    }

    int32_t latToPixelY(double lat, uint8_t zoom) {
        double latRad = lat * MAP_PI / 180.0;
        double y = (1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / MAP_PI) / 2.0 * (1 << zoom) * TILE_SIZE;
        return (int32_t)y;
    }

    double tileYToLat(int32_t y, uint8_t zoom) {
        double n = MAP_PI - 2.0 * MAP_PI * y / (1 << zoom);
        return 180.0 / MAP_PI * atan(0.5 * (exp(n) - exp(-n)));
    }

    double tileXToLon(int32_t x, uint8_t zoom) {
        return x / (double)(1 << zoom) * 360.0 - 180.0;
    }
}

void MapRenderer::init(lv_obj_t* parent) {
    // Create LVGL canvas for map rendering (full display size)
    _canvasBuf = (lv_color_t*)ps_malloc(DISP_W * DISP_H * sizeof(lv_color_t));
    if (!_canvasBuf) {
        Serial.println("MapRenderer: PSRAM alloc failed for canvas!");
        return;
    }

    _canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(_canvas, _canvasBuf, DISP_W, DISP_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(_canvas, 0, 0);

    // Fill with dark background initially
    lv_canvas_fill_bg(_canvas, lv_color_hex(0x1A1A2E), LV_OPA_COVER);
}

void MapRenderer::setCenter(double lat, double lon) {
    _centerLat = lat;
    _centerLon = lon;
}

void MapRenderer::setZoom(uint8_t z) {
    if (z < 12) z = 12;
    if (z > 16) z = 16;
    _zoom = z;
}

void MapRenderer::panBy(int16_t dx, int16_t dy) {
    // Convert pixel offset to lat/lon change
    double scale = (double)(1 << _zoom) * TILE_SIZE;
    _centerLon -= (double)dx / scale * 360.0;

    // For latitude, use inverse Mercator
    int32_t cy = MapMath::latToPixelY(_centerLat, _zoom);
    cy += dy;
    // Convert back to lat
    double tileY = (double)cy / TILE_SIZE;
    double n = MapMath::MAP_PI - 2.0 * MapMath::MAP_PI * tileY / (1 << _zoom);
    _centerLat = 180.0 / MapMath::MAP_PI * atan(0.5 * (exp(n) - exp(-n)));
}

void MapRenderer::zoomIn() {
    if (_zoom < 16) _zoom++;
}

void MapRenderer::zoomOut() {
    if (_zoom > 12) _zoom--;
}

void MapRenderer::setRoute(const RoutePolyline* route) {
    _route = route;
}

void MapRenderer::setHeading(float heading) {
    _heading = heading;
}

void MapRenderer::drawTile(int32_t tileX, int32_t tileY, int16_t offsetX, int16_t offsetY) {
    const CachedTile* tile = tileReader.getTile(tileX, tileY, _zoom);
    if (!tile || !tile->pixels) {
        // Draw placeholder (dark tile with grid)
        for (int y = 0; y < TILE_SIZE && (offsetY + y) < DISP_H; y++) {
            for (int x = 0; x < TILE_SIZE && (offsetX + x) < DISP_W; x++) {
                int px = offsetX + x;
                int py = offsetY + y;
                if (px >= 0 && py >= 0) {
                    // Dark background with subtle grid
                    uint16_t c;
                    if (x % 32 == 0 || y % 32 == 0) {
                        c = lv_color_hex(0x2A2A3E).full;
                    } else {
                        c = lv_color_hex(0x1A1A2E).full;
                    }
                    _canvasBuf[py * DISP_W + px].full = c;
                }
            }
        }
        return;
    }

    _hasData = true;

    // Copy tile pixels to canvas at offset
    for (int y = 0; y < TILE_SIZE; y++) {
        int py = offsetY + y;
        if (py < 0 || py >= DISP_H) continue;
        for (int x = 0; x < TILE_SIZE; x++) {
            int px = offsetX + x;
            if (px < 0 || px >= DISP_W) continue;
            _canvasBuf[py * DISP_W + px].full = tile->pixels[y * TILE_SIZE + x];
        }
    }
}

void MapRenderer::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                            uint16_t color, uint8_t width) {
    // Bresenham's line with thickness
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        // Draw a thick point
        int halfW = width / 2;
        for (int wy = -halfW; wy <= halfW; wy++) {
            for (int wx = -halfW; wx <= halfW; wx++) {
                int px = x0 + wx;
                int py = y0 + wy;
                if (px >= 0 && px < DISP_W && py >= 0 && py < DISP_H) {
                    _canvasBuf[py * DISP_W + px].full = color;
                }
            }
        }

        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void MapRenderer::drawRoute() {
    if (!_route || _route->count < 2) return;

    // Convert route points to screen coordinates
    int32_t centerPx = MapMath::lonToPixelX(_centerLon, _zoom);
    int32_t centerPy = MapMath::latToPixelY(_centerLat, _zoom);
    int32_t screenCx = DISP_W / 2;
    int32_t screenCy = DISP_H / 2;

    // Route color: vibrant blue (#4285F4 Google blue)
    uint16_t routeColor = lv_color_hex(0x4285F4).full;

    for (uint16_t i = 1; i < _route->count; i++) {
        int32_t px0 = MapMath::lonToPixelX(_route->points[i-1].lon, _zoom);
        int32_t py0 = MapMath::latToPixelY(_route->points[i-1].lat, _zoom);
        int32_t px1 = MapMath::lonToPixelX(_route->points[i].lon, _zoom);
        int32_t py1 = MapMath::latToPixelY(_route->points[i].lat, _zoom);

        int16_t sx0 = (int16_t)(px0 - centerPx + screenCx);
        int16_t sy0 = (int16_t)(py0 - centerPy + screenCy);
        int16_t sx1 = (int16_t)(px1 - centerPx + screenCx);
        int16_t sy1 = (int16_t)(py1 - centerPy + screenCy);

        // Only draw if at least partially on screen
        if ((sx0 >= -50 && sx0 < DISP_W + 50 && sy0 >= -50 && sy0 < DISP_H + 50) ||
            (sx1 >= -50 && sx1 < DISP_W + 50 && sy1 >= -50 && sy1 < DISP_H + 50)) {
            drawLine(sx0, sy0, sx1, sy1, routeColor, 3);
        }
    }
}

void MapRenderer::drawPositionMarker() {
    // Draw a bright cyan dot at center with pulsing ring
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;
    uint16_t markerColor = lv_color_hex(0x00E5FF).full;
    uint16_t ringColor = lv_color_hex(0x0088AA).full;

    // Outer ring
    for (int a = 0; a < 360; a += 5) {
        float rad = a * 3.14159f / 180.0f;
        int rx = cx + (int)(8 * cos(rad));
        int ry = cy + (int)(8 * sin(rad));
        if (rx >= 0 && rx < DISP_W && ry >= 0 && ry < DISP_H) {
            _canvasBuf[ry * DISP_W + rx].full = ringColor;
        }
    }

    // Inner dot (radius 4)
    for (int dy = -4; dy <= 4; dy++) {
        for (int dx = -4; dx <= 4; dx++) {
            if (dx * dx + dy * dy <= 16) {
                int px = cx + dx;
                int py = cy + dy;
                if (px >= 0 && px < DISP_W && py >= 0 && py < DISP_H) {
                    _canvasBuf[py * DISP_W + px].full = markerColor;
                }
            }
        }
    }

    // Heading indicator (small triangle)
    if (_heading > 0) {
        float rad = (_heading - 90) * 3.14159f / 180.0f;
        int tx = cx + (int)(12 * cos(rad));
        int ty = cy + (int)(12 * sin(rad));
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                int px = tx + dx;
                int py = ty + dy;
                if (px >= 0 && px < DISP_W && py >= 0 && py < DISP_H) {
                    _canvasBuf[py * DISP_W + px].full = markerColor;
                }
            }
        }
    }
}

void MapRenderer::drawAttribution() {
    // Draw "Google Maps" text at bottom-right of canvas
    // Using simple pixel text since we're drawing directly to canvas buffer
    // The LVGL label overlay handles this more cleanly — see UI code
}

void MapRenderer::render() {
    if (!_canvasBuf) return;

    // Calculate which tiles we need
    int32_t centerPx = MapMath::lonToPixelX(_centerLon, _zoom);
    int32_t centerPy = MapMath::latToPixelY(_centerLat, _zoom);

    // Center tile
    int32_t centerTileX = centerPx / TILE_SIZE;
    int32_t centerTileY = centerPy / TILE_SIZE;

    // Pixel offset within center tile
    int32_t offsetInTileX = centerPx % TILE_SIZE;
    int32_t offsetInTileY = centerPy % TILE_SIZE;

    // Screen position of center tile's top-left corner
    int16_t baseScrX = (DISP_W / 2) - offsetInTileX;
    int16_t baseScrY = (DISP_H / 2) - offsetInTileY;

    // We need tiles from (centerTile - 1) to (centerTile + 1) in both axes
    // to cover the full 320x240 viewport
    for (int ty = -1; ty <= 2; ty++) {
        for (int tx = -1; tx <= 2; tx++) {
            int16_t scrX = baseScrX + tx * TILE_SIZE;
            int16_t scrY = baseScrY + ty * TILE_SIZE;

            // Skip tiles completely off-screen
            if (scrX + TILE_SIZE < 0 || scrX >= DISP_W) continue;
            if (scrY + TILE_SIZE < 0 || scrY >= DISP_H) continue;

            drawTile(centerTileX + tx, centerTileY + ty, scrX, scrY);
        }
    }

    // Draw overlays
    drawRoute();
    drawPositionMarker();

    // Invalidate the LVGL canvas to trigger redraw
    lv_obj_invalidate(_canvas);
}
