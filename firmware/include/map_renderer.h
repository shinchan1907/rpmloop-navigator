// Map renderer for RPMLOOP Navigator
// Converts lat/lon to tile coordinates, renders 2x2+ viewport from SD tiles,
// draws route polyline overlay, handles smooth panning

#pragma once

#include <lvgl.h>
#include "tile_reader.h"
#include "ble_data.h"

// Web Mercator math
namespace MapMath {
    // Convert latitude to tile Y at given zoom
    int32_t latToTileY(double lat, uint8_t zoom);
    // Convert longitude to tile X at given zoom
    int32_t lonToTileX(double lon, uint8_t zoom);
    // Convert lat to pixel Y within tile
    int32_t latToPixelY(double lat, uint8_t zoom);
    // Convert lon to pixel X within tile
    int32_t lonToPixelX(double lon, uint8_t zoom);
    // Convert tile coords back to lat/lon (top-left corner)
    double tileYToLat(int32_t y, uint8_t zoom);
    double tileXToLon(int32_t x, uint8_t zoom);
}

class MapRenderer {
public:
    // Initialize the map renderer with an LVGL canvas
    void init(lv_obj_t* parent);

    // Set the center position and zoom
    void setCenter(double lat, double lon);
    void setZoom(uint8_t z);
    uint8_t getZoom() const { return _zoom; }

    // Pan by pixel offset (for touch dragging)
    void panBy(int16_t dx, int16_t dy);

    // Zoom in/out
    void zoomIn();
    void zoomOut();

    // Render the current viewport (call when position/zoom changes)
    void render();

    // Set route polyline to draw
    void setRoute(const RoutePolyline* route);

    // Set heading for rotation indicator
    void setHeading(float heading);

    // Get the LVGL canvas object
    lv_obj_t* getCanvas() { return _canvas; }

    // Is map data available?
    bool hasMapData() const { return _hasData; }

private:
    lv_obj_t* _canvas = nullptr;
    lv_color_t* _canvasBuf = nullptr;

    double _centerLat = 0.0;
    double _centerLon = 0.0;
    uint8_t _zoom = 15;
    float _heading = 0.0f;
    bool _hasData = false;

    const RoutePolyline* _route = nullptr;

    // Draw a single tile onto the canvas buffer at pixel offset
    void drawTile(int32_t tileX, int32_t tileY, int16_t offsetX, int16_t offsetY);

    // Draw route polyline overlay
    void drawRoute();

    // Draw position marker
    void drawPositionMarker();

    // Draw "Google Maps" attribution
    void drawAttribution();

    // Draw a line on the canvas (for route)
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, 
                  uint16_t color, uint8_t width);
};

extern MapRenderer mapRenderer;
