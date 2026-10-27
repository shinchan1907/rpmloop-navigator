# RPMLOOP display lab

An interactive **browser mockup**, not ESP32 firmware. Open `index.html` in Chrome,
Edge or Firefox; keep `preview.css`, `preview.js`, and `data/jaipur.js` beside it.
No installation, account, API key, tile server, or Internet connection is required.
For a local HTTP preview, run `python -m http.server 8080` at the repository root
and open `http://localhost:8080/docs/preview/`.

## Interaction

| Control | Behavior |
|---|---|
| Drag map / arrow keys while map is focused | Pan the map; stops following the rider |
| + / − buttons, mouse wheel, + / − keys | Zoom 14–18 |
| Crosshair / R while map is focused | Recenter and follow the rider |
| Play / pause | Move along the road-following demo geometry |
| Speed selector | 1×, 4× or 12× simulated playback |
| Progress slider | Scrub the ride, including arrival |
| Maneuver list | Jump to a turn and pause |
| Day / night | Change map and guidance palette |
| Orientation | 320 × 240 landscape or 240 × 320 portrait |
| Preview scale | Enlarge the case; LCD pixel dimensions stay fixed |
| Export LCD PNG | Download native-resolution artwork for a render |

The canvas has exactly 76,800 pixels in either orientation. The CSS case is an
illustration of the P3 front enclosure and mount, not a dimensionally accurate
CAD viewer. Landscape matches the existing front opening; the portrait case is
an alternate presentation, not a new manufacturing revision. Preview scaling
is not a physical 2.8-inch calibration on the user's monitor.

## Map and route provenance

The bundled snapshot covers central Jaipur, India, from **75.807° to 75.841° E**
and **26.900° to 26.937° N**. It was retrieved on **27 October 2026**. Map panning
is bounded to this demo area; it is not a worldwide map browser.

- Map geometry: © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright),
  [Open Database License 1.0](https://opendatacommons.org/licenses/odbl/1-0/).
  `data/jaipur.js` contains the attributed, simplified database snapshot (way IDs,
  selected tags and longitude/latitude geometry); its database portion remains ODbL.
  Map-derived screen exports and product renders carry the same attribution here
  and in the repository README. Keep this credit with redistributed artwork.
- Source query: Overpass API, `way[~"highway|building|leisure|landuse|natural|waterway"~"."](26.900,75.807,26.937,75.841);out geom;`
- Route: [OSRM](https://project-osrm.org/), `route/v1/driving`, full GeoJSON geometry
  and steps from the Albert Hall Museum area to Hawa Mahal (about 1.95 km).
  The driving profile is an illustrative source, not motorcycle route validation.
  Unnamed roads are shown as such. The last waypoint is snapped to Amer Road near
  Hawa Mahal, not a claim of vehicle access to the monument entrance.

The browser rasterizes the bundled geometry into cached 256 × 256 Web Mercator
tiles using a small map palette, then draws a route overlay, rider marker, turn
banner and trip footer. It does **not** download or redistribute raster tiles
from the public OpenStreetMap tile server. Street names may be omitted to keep
the tiny screen readable. Route distance and maneuver timing come from the saved
snapshot; the displayed speed (36 km/h), playback and connection label are demo UI.
Remaining time is an estimate using the saved OSRM route duration, not traffic data.

## Plausible hardware concept, with limits

This explores **phone-prepared map tiles and route geometry, displayed locally on
an ESP32-S3 TFT**, with touch zoom/pan. It uses simple flat map features, a short
route and a readable overlay, rather than satellite imagery or 3D buildings.
Actual tile format, storage, BLE transfer, display bandwidth, memory use, touch
calibration and frame rate still require implementation and measurement. The
browser rendering strategy and full bundled dataset are not a proposed firmware
memory layout. No hardware performance or feature availability is demonstrated.

No firmware, companion app, live GPS, BLE transport, search, rerouting or traffic
implementation is included. There are no browser network calls during use.

## Product renders

The day and night exports in `../../rendering/map-ui/` are native 320 × 240 PNGs
from this mockup. `../../rendering/render_map_product.py` loads the original CAD-
derived Blender scene, hides the previous arrow artwork and adds a correctly
proportioned 4:3 screen illustration. It writes **new** image and scene filenames.
The original `hero.png`, `exploded.png`, `port-and-mount.png`, render script and
`rpmloop_studio.blend` are retained. Physical active-area dimensions remain
unverified; the render-only screen plane does not change enclosure CAD or print files.
