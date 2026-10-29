#ifndef MAP_VIEW_H
#define MAP_VIEW_H

#include <lvgl.h>

// ==============================================================================
// MapView — reusable procedural map component (NAV + MAPS screens).
// Draws a Google-Maps-like light map on an LVGL canvas: road grid with
// casing, park/water blobs, street labels, the route polyline (white casing
// + #1A73E8 line) and the position arrow.
// SEAM: all drawing happens inside render() from a tiny world model. A
// future TileStore (SD-card OSM tiles) can replace render()'s body without
// changing the screens — they only use create/setPan/setZoom/setLocked.
// The canvas buffer lives in PSRAM (153 KB at 320x240).
// ==============================================================================
class MapView {
public:
    MapView();
    ~MapView();

    void create(lv_obj_t* parent, int x, int y, int w, int h);
    void setPan(int dx, int dy);        // relative pan; unlocks the map
    void setZoom(int level);            // 14..18
    void setLocked(bool locked);        // locked = follow position, pan = 0
    bool locked() const { return _locked; }
    int  zoom() const { return _zoom; }
    void render();

private:
    lv_obj_t*   _canvas;
    lv_color_t* _buf;
    int _w, _h;
    int _panX, _panY;
    int _zoom;
    bool _locked;

    float scale() const;
    int sx(int worldX) const;
    int sy(int worldY) const;
    void drawLine(int x0, int y0, int x1, int y1, uint32_t colorHex, int width);
    void drawPoly(const int pts[][2], int count, uint32_t colorHex);
    void drawRoute();
    void drawArrow();
};

#endif // MAP_VIEW_H
