# LCD artwork

`lcd-day.png` and `lcd-night.png` are native **320 × 240** exports from the
[interactive mockup](../../docs/preview/), at its initial 67% ride position,
zoom 16, in landscape orientation. Use **Export LCD PNG** after changing the
theme to reproduce each texture. The browser controls are also drawn into the
export so the screen in Blender matches the mockup.

Map data and derived artwork: © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright),
[ODbL 1.0](https://opendatacommons.org/licenses/odbl/1-0/).
Route: OSRM driving-profile snapshot, Jaipur, retrieved 27 October 2026.
These are UI concept assets, not evidence of running firmware.

From the repository root, regenerate new product images with:

```sh
blender --background --python rendering/render_map_product.py
```

This writes `hero-map-day.png`, `hero-map-night.png`, `display-map-detail.png`
and `exploded-map.png` into `docs/images/`, plus `rpmloop_map_studio.blend`
into `rendering/`. The original images and scene are preserved. To preview
the assembled model in Blender, open the map scene and select its camera view.
