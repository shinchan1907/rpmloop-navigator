package io.bytenex.rpmloop.nav

import kotlin.math.asin
import kotlin.math.atan
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sinh
import kotlin.math.sqrt
import kotlin.math.tan

/** A WGS84 coordinate. Field order is (lat, lon) everywhere in this app;
 *  OSRM/GeoJSON use (lon, lat) — conversion happens in OsrmClient only. */
data class GeoPoint(val lat: Double, val lon: Double)

/** Small geodesy helpers: distances, polyline projection, slippy-map tiles. */
object GeoMath {

    const val EARTH_RADIUS_M = 6_371_000.0

    fun haversineM(a: GeoPoint, b: GeoPoint): Double {
        val dLat = Math.toRadians(b.lat - a.lat)
        val dLon = Math.toRadians(b.lon - a.lon)
        val lat1 = Math.toRadians(a.lat)
        val lat2 = Math.toRadians(b.lat)
        val h = sin(dLat / 2).pow(2) + cos(lat1) * cos(lat2) * sin(dLon / 2).pow(2)
        return 2 * EARTH_RADIUS_M * asin(sqrt(h).coerceIn(0.0, 1.0))
    }

    /** Cumulative distance (metres) at each vertex of [points]. */
    fun cumulativeDistances(points: List<GeoPoint>): List<Double> {
        if (points.isEmpty()) return emptyList()
        val out = ArrayList<Double>(points.size)
        var acc = 0.0
        out.add(0.0)
        for (i in 1 until points.size) {
            acc += haversineM(points[i - 1], points[i])
            out.add(acc)
        }
        return out
    }

    data class Projection(
        /** Distance along the polyline of the closest point, metres. */
        val alongM: Double,
        /** Perpendicular distance from the polyline, metres. */
        val crossTrackM: Double,
        /** Index of the segment the closest point lies on. */
        val segmentIndex: Int
    )

    /**
     * Projects [p] onto the polyline using a local equirectangular frame per
     * segment (accurate at the scales this app works at — a few hundred
     * metres). [cumulative] must be GeoMath.cumulativeDistances(points).
     */
    fun projectOntoPolyline(
        p: GeoPoint,
        points: List<GeoPoint>,
        cumulative: List<Double>
    ): Projection {
        if (points.isEmpty()) return Projection(0.0, Double.MAX_VALUE, 0)
        if (points.size == 1) {
            return Projection(0.0, haversineM(p, points[0]), 0)
        }
        var bestDist = Double.MAX_VALUE
        var bestAlong = 0.0
        var bestSeg = 0
        for (i in 0 until points.size - 1) {
            val a = points[i]
            val b = points[i + 1]
            // Local frame: x east, y north, metres, origin at a.
            val kx = Math.toRadians(1.0) * EARTH_RADIUS_M * cos(Math.toRadians(a.lat))
            val ky = Math.toRadians(1.0) * EARTH_RADIUS_M
            val bx = (b.lon - a.lon) * kx
            val by = (b.lat - a.lat) * ky
            val px = (p.lon - a.lon) * kx
            val py = (p.lat - a.lat) * ky
            val segLen2 = bx * bx + by * by
            val t = if (segLen2 <= 0.0) {
                0.0
            } else {
                ((px * bx + py * by) / segLen2).coerceIn(0.0, 1.0)
            }
            val dx = px - t * bx
            val dy = py - t * by
            val dist = sqrt(dx * dx + dy * dy)
            if (dist < bestDist) {
                bestDist = dist
                bestSeg = i
                val segLen = cumulative[i + 1] - cumulative[i]
                bestAlong = cumulative[i] + t * segLen
            }
        }
        return Projection(bestAlong, bestDist, bestSeg)
    }

    // ---- slippy-map tile math (standard OSM/Google tile scheme) --------------

    fun lonToTileX(lon: Double, z: Int): Int {
        val n = (1 shl z).toDouble()
        return floor((lon + 180.0) / 360.0 * n).toInt().coerceIn(0, (1 shl z) - 1)
    }

    fun latToTileY(lat: Double, z: Int): Int {
        val n = (1 shl z).toDouble()
        val latRad = Math.toRadians(lat.coerceIn(-85.0511, 85.0511))
        val y = (1.0 - kotlin.math.ln(tan(latRad) + 1.0 / cos(latRad)) / Math.PI) / 2.0 * n
        return floor(y).toInt().coerceIn(0, (1 shl z) - 1)
    }

    /** Centre coordinate of tile (z, x, y). */
    fun tileCentre(z: Int, x: Int, y: Int): GeoPoint {
        val n = (1 shl z).toDouble()
        val lon = (x + 0.5) / n * 360.0 - 180.0
        val latRad = atan(sinh(Math.PI * (1.0 - 2.0 * (y + 0.5) / n)))
        return GeoPoint(Math.toDegrees(latRad), lon)
    }

    /** Ground width (and height) of one 256px tile at [lat], metres. */
    fun tileGroundSizeM(lat: Double, z: Int): Double =
        156543.03392 * cos(Math.toRadians(lat)) / (1 shl z).toDouble() * 256.0

    /** Linear interpolation between two coordinates (short distances only). */
    fun lerp(a: GeoPoint, b: GeoPoint, t: Double): GeoPoint =
        GeoPoint(a.lat + (b.lat - a.lat) * t, a.lon + (b.lon - a.lon) * t)
}
