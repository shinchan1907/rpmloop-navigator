#!/usr/bin/env python3
"""
RPMLOOP Navigator — Offline Map Tile Downloader
Downloads OpenStreetMap tiles for a target region (default: Jalandhar, Punjab)
and saves them as 256x256 JPEG files in /tiles/{z}/{x}/{y}.jpg layout for FAT32 MicroSD cards.
"""

import os
import sys
import math
import time
import argparse
import urllib.request
from io import BytesIO
from PIL import Image

def deg2num(lat_deg, lon_deg, zoom):
    lat_rad = math.radians(lat_deg)
    n = 2.0 ** zoom
    xtile = int((lon_deg + 180.0) / 360.0 * n)
    ytile = int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n)
    return (xtile, ytile)

def download_tiles(min_lat, max_lat, min_lon, max_lon, zooms, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    
    # CartoDB Positron tiles (clean, high contrast, fast CDN, permissive OSM terms)
    url_template = "https://a.basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}.png"
    headers = {
        "User-Agent": "RPMLOOP-Navigator/3.0 (Offline Map Preparation; contact@rpmloop.dev)"
    }
    
    total_downloaded = 0
    total_skipped = 0

    print("==================================================")
    print("   RPMLOOP Navigator — Tile Downloader   ")
    print("==================================================")
    print(f"BBox: Lat [{min_lat}, {max_lat}], Lon [{min_lon}, {max_lon}]")
    print(f"Zoom levels: {zooms}")
    print(f"Output directory: {os.path.abspath(out_dir)}")
    print("--------------------------------------------------")

    for z in zooms:
        x_min, y_max = deg2num(min_lat, min_lon, z)
        x_max, y_min = deg2num(max_lat, max_lon, z)

        # Handle coordinate ordering
        if x_min > x_max: x_min, x_max = x_max, x_min
        if y_min > y_max: y_min, y_max = y_max, y_min

        count_z = (x_max - x_min + 1) * (y_max - y_min + 1)
        print(f"\n[Zoom {z}] Tiles: X [{x_min}..{x_max}], Y [{y_min}..{y_max}] -> Total {count_z} tiles")

        for x in range(x_min, x_max + 1):
            for y in range(y_min, y_max + 1):
                tile_dir = os.path.join(out_dir, str(z), str(x))
                os.makedirs(tile_dir, exist_ok=True)
                tile_path = os.path.join(tile_dir, f"{y}.jpg")

                if os.path.exists(tile_path) and os.path.getsize(tile_path) > 500:
                    total_skipped += 1
                    continue

                url = url_template.format(z=z, x=x, y=y)
                req = urllib.request.Request(url, headers=headers)
                try:
                    with urllib.request.urlopen(req, timeout=10) as resp:
                        img_data = resp.read()
                        img = Image.open(BytesIO(img_data)).convert("RGB")
                        img.save(tile_path, "JPEG", quality=82, optimize=True)
                        total_downloaded += 1
                        print(f"  Downloaded: {z}/{x}/{y}.jpg ({len(img_data)//1024} KB)")
                        time.sleep(0.05)  # Be polite to tile server
                except Exception as e:
                    print(f"  Error downloading {z}/{x}/{y}: {e}")
                    time.sleep(0.2)

    print("\n==================================================")
    print(f"Done! Downloaded: {total_downloaded}, Skipped (existing): {total_skipped}")
    print(f"Tiles saved to: {os.path.abspath(out_dir)}")
    print("Copy the 'tiles' folder directly to the root of your FAT32 MicroSD card!")
    print("==================================================")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Download OSM map tiles for RPMLOOP Navigator")
    # Default: Jalandhar city & immediate outskirts (Punjab)
    parser.add_argument("--min-lat", type=float, default=31.26, help="South latitude")
    parser.add_argument("--max-lat", type=float, default=31.38, help="North latitude")
    parser.add_argument("--min-lon", type=float, default=75.50, help="West longitude")
    parser.add_argument("--max-lon", type=float, default=75.64, help="East longitude")
    parser.add_argument("--zooms", type=int, nargs="+", default=[13, 14, 15], help="Zoom levels (default: 13 14 15)")
    parser.add_argument("--out", type=str, default="tiles", help="Output directory")

    args = parser.parse_args()
    download_tiles(args.min_lat, args.max_lat, args.min_lon, args.max_lon, args.zooms, args.out)
