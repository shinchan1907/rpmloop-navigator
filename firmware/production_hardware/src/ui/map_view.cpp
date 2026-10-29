#include <stdlib.h>
#include "esp_heap_caps.h"
#include "ui/map_view.h"
#include "ui/ui_common.h"

// World model: a 960x720 "city" in world units. The route runs north through
// the middle; the view centre sits on the route so the arrow anchor
// (bottom-centre of the view) is on the route when locked.
static const int kWorldCx = 480;
static const int kWorldCy = 430;

static const int kRoute[][2] = {
    {480, 715}, {480, 560}, {520, 470}, {610, 430},
    {640, 330}, {600, 240}, {660, 150}, {700, 40},
};
static const int kRouteN = sizeof(kRoute) / sizeof(kRoute[0]);

static const int kPark1[][2] = {{110, 120}, {250, 100}, {290, 190}, {200, 250}, {120, 220}};
static const int kPark2[][2] = {{700, 500}, {820, 480}, {860, 560}, {760, 610}, {690, 570}};
static const int kWater[][2] = {{0, 520}, {150, 540}, {190, 640}, {90, 720}, {0, 700}};

// River: a wide band sweeping diagonally across the whole city (drawn as a
// fat polyline, casing pass + core pass, like the reference nav view where
// the river cuts through the middle of the map).
static const int kRiver[][2] = {
    {120, 0}, {260, 140}, {330, 300}, {300, 470}, {400, 720},
};
static const int kRiverN = sizeof(kRiver) / sizeof(kRiver[0]);

// City building blocks: {worldX, worldY, w, h, shade} axis-aligned rects in
// world coords, clustered along the road grid for the built-up feel.
struct Building { int x, y, w, h, shade; };
static const Building kBuildings[] = {
    {330, 150, 70, 52, 0}, {420, 130, 56, 44, 1}, {545, 165, 66, 50, 0},
    {640, 120, 52, 66, 1}, {180, 320, 62, 48, 1}, {330, 345, 50, 40, 0},
    {700, 300, 72, 54, 0}, {790, 370, 54, 44, 1}, {150, 470, 58, 46, 0},
    {245, 585, 68, 50, 1}, {560, 560, 52, 64, 0}, {835, 195, 60, 48, 0},
};
static const int kBuildingN = sizeof(kBuildings) / sizeof(kBuildings[0]);

MapView::MapView()
    : _canvas(nullptr), _buf(nullptr), _w(0), _h(0),
      _panX(0), _panY(0), _zoom(16), _locked(true),
      _dragLastX(0), _dragLastY(0), _dragging(false) {}

MapView::~MapView() { if (_buf) free(_buf); }

void MapView::create(lv_obj_t* parent, int x, int y, int w, int h) {
    _w = w; _h = h;
    size_t bytes = (size_t)w * (size_t)h * sizeof(lv_color_t);
    _buf = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_buf) _buf = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    _canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(_canvas, _buf, w, h, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(_canvas, x, y);
    // Touch drag pans the map (see drag_event_cb); the canvas must be
    // clickable for the pointer indev to deliver press events to it.
    lv_obj_add_flag(_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(_canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(_canvas, drag_event_cb, LV_EVENT_ALL, this);
    render();
}

// Finger content-follow panning: sx()/sy() subtract the pan from the world
// centre, so moving the content WITH the finger means pan -= finger delta.
void MapView::drag_event_cb(lv_event_t* e) {
    MapView* self = (MapView*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING &&
        code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    if (code == LV_EVENT_PRESSED) {
        self->_dragging = true;
        self->_dragLastX = p.x;
        self->_dragLastY = p.y;
    } else if (code == LV_EVENT_PRESSING && self->_dragging) {
        int dx = p.x - self->_dragLastX;
        int dy = p.y - self->_dragLastY;
        self->_dragLastX = p.x;
        self->_dragLastY = p.y;
        if (dx != 0 || dy != 0) self->setPan(-dx, -dy);
    } else {  // RELEASED / PRESS_LOST
        self->_dragging = false;
    }
}

float MapView::scale() const {
    static const float kScales[] = {0.5f, 0.71f, 1.0f, 1.41f, 2.0f};  // z14..z18
    int idx = _zoom - 14;
    if (idx < 0) idx = 0;
    if (idx > 4) idx = 4;
    return kScales[idx];
}

int MapView::sx(int worldX) const {
    return (int)((worldX - (kWorldCx + _panX)) * scale()) + _w / 2;
}
int MapView::sy(int worldY) const {
    return (int)((worldY - (kWorldCy + _panY)) * scale()) + (_h - 35);
}

void MapView::drawLine(int x0, int y0, int x1, int y1, uint32_t colorHex, int width) {
    lv_point_t pts[2] = {{(lv_coord_t)x0, (lv_coord_t)y0}, {(lv_coord_t)x1, (lv_coord_t)y1}};
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(colorHex);
    dsc.width = width;
    dsc.round_start = 1;
    dsc.round_end = 1;
    lv_canvas_draw_line(_canvas, pts, 2, &dsc);
}

void MapView::drawPoly(const int pts[][2], int count, uint32_t colorHex) {
    lv_point_t* poly = (lv_point_t*)alloca(sizeof(lv_point_t) * count);
    for (int i = 0; i < count; i++) {
        poly[i].x = (lv_coord_t)sx(pts[i][0]);
        poly[i].y = (lv_coord_t)sy(pts[i][1]);
    }
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(colorHex);
    dsc.bg_opa = LV_OPA_COVER;
    lv_canvas_draw_polygon(_canvas, poly, count, &dsc);
}

void MapView::drawRiver() {
    // Fat polyline: slightly deeper edge pass, then the water core.
    for (int pass = 0; pass < 2; pass++) {
        uint32_t col = pass == 0 ? 0x9CC8DC : COL_WATER;
        int width = (int)((pass == 0 ? 104 : 92) * scale());
        if (width < 12) width = 12;
        for (int i = 0; i + 1 < kRiverN; i++)
            drawLine(sx(kRiver[i][0]), sy(kRiver[i][1]),
                     sx(kRiver[i + 1][0]), sy(kRiver[i + 1][1]), col, width);
    }
}

void MapView::drawBuildings() {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    float sc = scale();
    for (int i = 0; i < kBuildingN; i++) {
        const Building& bl = kBuildings[i];
        int x0 = sx(bl.x), y0 = sy(bl.y + bl.h);   // sy() grows downward
        lv_area_t a;
        a.x1 = (lv_coord_t)x0;
        a.y1 = (lv_coord_t)y0;
        a.x2 = (lv_coord_t)(x0 + (int)(bl.w * sc));
        a.y2 = (lv_coord_t)(y0 + (int)(bl.h * sc));
        if (a.x2 < 0 || a.x1 >= _w || a.y2 < 0 || a.y1 >= _h) continue;
        dsc.bg_color = lv_color_hex(bl.shade ? COL_BUILDING_2 : COL_BUILDING);
        lv_canvas_draw_rect(_canvas, a.x1, a.y1, a.x2 - a.x1, a.y2 - a.y1, &dsc);
    }
}

void MapView::drawRoute() {
    // White casing first, then the Google-blue core line.
    for (int pass = 0; pass < 2; pass++) {
        uint32_t col = pass == 0 ? COL_ROAD : COL_ROUTE;
        int width = (int)((pass == 0 ? 13 : 8) * scale());
        if (width < 4) width = 4;
        for (int i = 0; i + 1 < kRouteN; i++) {
            drawLine(sx(kRoute[i][0]), sy(kRoute[i][1]),
                     sx(kRoute[i + 1][0]), sy(kRoute[i + 1][1]), col, width);
        }
    }
}

void MapView::drawArrow() {
    // TomTom-style navigation chevron: larger two-tone arrow — white
    // outline triangle with a light-blue core. Anchor = bottom-centre of
    // the view, sitting on the route when locked.
    int ax = _w / 2;
    int ay = _h - 35;
    const int whiteTri[3][2] = {{ax, ay - 24}, {ax - 15, ay + 16}, {ax + 15, ay + 16}};
    const int blueTri[3][2]  = {{ax, ay - 17}, {ax - 10, ay + 12}, {ax + 10, ay + 12}};
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    lv_point_t pts[3];
    for (int i = 0; i < 3; i++) { pts[i].x = (lv_coord_t)whiteTri[i][0]; pts[i].y = (lv_coord_t)whiteTri[i][1]; }
    dsc.bg_color = lv_color_hex(COL_ROAD);
    lv_canvas_draw_polygon(_canvas, pts, 3, &dsc);
    for (int i = 0; i < 3; i++) { pts[i].x = (lv_coord_t)blueTri[i][0]; pts[i].y = (lv_coord_t)blueTri[i][1]; }
    dsc.bg_color = lv_color_hex(COL_CHEVRON);
    lv_canvas_draw_polygon(_canvas, pts, 3, &dsc);
}

void MapView::render() {
    if (!_canvas || !_buf) return;
    lv_canvas_fill_bg(_canvas, lv_color_hex(COL_MAP_BG), LV_OPA_COVER);

    drawPoly(kPark1, 5, COL_PARK);
    drawPoly(kPark2, 5, COL_PARK);
    drawPoly(kWater, 5, COL_WATER);
    drawRiver();        // wide river band across the city
    drawBuildings();    // pale-grey blocks (under the road network)

    // Road grid (world coords), casing pass then surface pass.
    float sc = scale();
    int casingW = (int)(10 * sc); if (casingW < 3) casingW = 3;
    int roadW = (int)(7 * sc);    if (roadW < 2) roadW = 2;
    for (int pass = 0; pass < 2; pass++) {
        uint32_t col = pass == 0 ? COL_ROAD_EDGE : COL_ROAD;
        int wdt = pass == 0 ? casingW : roadW;
        for (int wx = 60; wx <= 900; wx += 120)
            drawLine(sx(wx), sy(0), sx(wx), sy(720), col, wdt);
        for (int wy = 60; wy <= 660; wy += 120)
            drawLine(sx(0), sy(wy), sx(960), sy(wy), col, wdt);
        // Two diagonals for a less mechanical city feel.
        drawLine(sx(0), sy(600), sx(500), sy(120), col, wdt);
        drawLine(sx(350), sy(720), sx(960), sy(300), col, wdt);
    }

    drawRoute();

    // Street labels (skip when they'd land far off-view).
    struct Label { int x, y; const char* text; };
    static const Label kLabels[] = {
        {505, 448, "Grand Promenade"}, {300, 288, "Station Road"},
        {628, 610, "Canal Road"},      {128, 428, "Mall Road"},
    };
    lv_draw_label_dsc_t ldsc;
    lv_draw_label_dsc_init(&ldsc);
    ldsc.color = lv_color_hex(COL_MAP_LABEL);
    ldsc.font = &lv_font_montserrat_10;
    for (unsigned i = 0; i < sizeof(kLabels) / sizeof(kLabels[0]); i++) {
        int x = sx(kLabels[i].x), y = sy(kLabels[i].y);
        if (x > -80 && x < _w && y > -10 && y < _h)
            lv_canvas_draw_text(_canvas, x, y, 120, &ldsc, kLabels[i].text);
    }

    drawArrow();
    lv_obj_invalidate(_canvas);
}

void MapView::setPan(int dx, int dy) {
    _panX += dx;
    _panY += dy;
    if (_panX > 260) _panX = 260;
    if (_panX < -260) _panX = -260;
    if (_panY > 200) _panY = 200;
    if (_panY < -200) _panY = -200;
    _locked = false;
    render();
}

void MapView::setZoom(int level) {
    if (level < 14) level = 14;
    if (level > 18) level = 18;
    if (level == _zoom) return;
    _zoom = level;
    render();
}

void MapView::setLocked(bool locked) {
    _locked = locked;
    if (locked) { _panX = 0; _panY = 0; }
    render();
}
