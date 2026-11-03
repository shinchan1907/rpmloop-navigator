package io.bytenex.rpmloop.tiles

import io.bytenex.rpmloop.nav.GeoMath
import io.bytenex.rpmloop.nav.GeoPoint
import kotlin.math.ceil

/** One slippy-map tile address. */
data class TileCoord(val z: Int, val x: Int, val y: Int) {
    /** Path of the tile inside the app files dir and on the device SD card. */
    fun relativePath(): String = "tiles/$z/$x/$y.jpg"

    /** Absolute path the device expects in file_begin messages. */
    fun devicePath(): String = "/tiles/$z/$x/$y.jpg"
}

/**
 * Works out which tiles a job needs. Tile set sizes are computed BEFORE any
 * download starts, so the UI can show an honest count + size estimate and
 * ask for confirmation.
 */
object TilePlanner {

    val ZOOMS: IntRange = 13..16

    /** Size estimate per tile, from observed Google roadmap tile averages. */
    fun estimateBytesPerTile(z: Int): Long = if (z <= 14) 6_144L else 10_240L

    fun estimateTotalBytes(tiles: Collection<TileCoord>): Long =
        tiles.sumOf { estimateBytesPerTile(it.z) }

    /**
     * Corridor around a route polyline: every tile the line passes through
     * plus a 1-tile margin on all sides, zooms 13–16. The polyline is
     * sampled at well under a tile width so no crossed tile is missed.
     */
    fun corridorTiles(points: List<GeoPoint>): List<TileCoord> {
        if (points.size < 2) return emptyList()
        val midLat = points[points.size / 2].lat
        val out = HashSet<TileCoord>()
        for (z in ZOOMS) {
            val stepM = (GeoMath.tileGroundSizeM(midLat, z) * 0.4).coerceAtLeast(20.0)
            fun addAround(p: GeoPoint) {
                val cx = GeoMath.lonToTileX(p.lon, z)
                val cy = GeoMath.latToTileY(p.lat, z)
                for (dx in -1..1) {
                    for (dy in -1..1) {
                        val x = cx + dx
                        val y = cy + dy
                        if (x in 0 until (1 shl z) && y in 0 until (1 shl z)) {
                            out.add(TileCoord(z, x, y))
                        }
                    }
                }
            }
            for (i in 0 until points.size - 1) {
                val a = points[i]
                val b = points[i + 1]
                val segLen = GeoMath.haversineM(a, b)
                val steps = ceil(segLen / stepM).toInt().coerceAtLeast(1)
                for (s in 0..steps) {
                    addAround(GeoMath.lerp(a, b, s.toDouble() / steps))
                }
            }
        }
        return out.sortedWith(compareBy({ it.z }, { it.x }, { it.y }))
    }

    /**
     * All tiles covering a [radiusM] disc around [centre], zooms 13–16.
     * A tile is included when its centre is within the radius plus half a
     * tile width, so the disc edge has no gaps.
     */
    fun nearbyTiles(centre: GeoPoint, radiusM: Double = 5_000.0): List<TileCoord> {
        val out = ArrayList<TileCoord>()
        val dLat = radiusM / 111_320.0
        val dLon = radiusM / (111_320.0 * kotlin.math.cos(Math.toRadians(centre.lat)))
        for (z in ZOOMS) {
            val x0 = GeoMath.lonToTileX(centre.lon - dLon, z)
            val x1 = GeoMath.lonToTileX(centre.lon + dLon, z)
            val y0 = GeoMath.latToTileY(centre.lat + dLat, z)
            val y1 = GeoMath.latToTileY(centre.lat - dLat, z)
            val limit = radiusM + GeoMath.tileGroundSizeM(centre.lat, z) * 0.5
            for (x in minOf(x0, x1)..maxOf(x0, x1)) {
                for (y in minOf(y0, y1)..maxOf(y0, y1)) {
                    if (GeoMath.haversineM(centre, GeoMath.tileCentre(z, x, y)) <= limit) {
                        out.add(TileCoord(z, x, y))
                    }
                }
            }
        }
        return out
    }
}
