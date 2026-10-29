#ifndef MAP_VIEW_H
#define MAP_VIEW_H

#include <lvgl.h>

// ==============================================================================
// MapView — reusable procedural map component (NAV + MAPS screens).
// Draws a Google/TomTom-like light 2D map on an LVGL canvas: road grid with
// casing, a wide river band, pale-grey building blocks, park/water blobs,
// street labels, the route polyline (white casing + #1A73E8 line) and a
// two-tone position chevron anchored at the bottom-centre.
// Touch: the canvas itself handles drag-to-pan (finger content-follow);
// screens only add zoom +/- and recentre buttons around it.
// SEAM: all drawing happens inside render() from a tiny world model; a
// different world renderer can replace render()'s body without changing
// the screens — they only use create/setPan/setZoom/setLocked.
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
    int _dragLastX, _dragLastY;
    bool _dragging;

    static void drag_event_cb(lv_event_t* e);
    float scale() const;
    int sx(int worldX) const;
    int sy(int worldY) const;
    void drawLine(int x0, int y0, int x1, int y1, uint32_t colorHex, int width);
    void drawPoly(const int pts[][2], int count, uint32_t colorHex);
    void drawRiver();
    void drawBuildings();
    void drawRoute();
    void drawArrow();
};

#endif // MAP_VIEW_H
