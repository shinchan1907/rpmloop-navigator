package io.bytenex.rpmloop.nav

import org.json.JSONArray
import org.json.JSONObject

/**
 * Builds the device-bound polyline messages for a [Route], in exactly the
 * spec's format (protocol_spec.json -> messages.polyline_chunk):
 *
 *   {"type":"poly","route_id":"r001","seq":0,"total":3,
 *    "points":[[31.326,75.576],[31.327,75.577], ...]}
 *
 * Points are [lat, lon] pairs at 6 decimal places. The device only draws
 * this line over its offline tiles, so the device copy is decimated to a
 * minimum 4 m vertex spacing (the engine keeps the full-resolution route
 * locally) and chunks are packed to fit the negotiated ATT payload
 * ([maxJsonBytes], = MTU - 3). Chunk order is significant; BleManager
 * sends them in order.
 */
object RoutePolylineProvider {

    private const val MIN_VERTEX_SPACING_M = 4.0
    /** Upper bound of one "[31.326015,75.576180]," point incl. separator. */
    private const val POINT_CHARS = 24
    /** JSON envelope around the points array. */
    private const val ENVELOPE_CHARS = 70

    /** Returns the ordered list of "poly" JSON messages for [route]. */
    fun buildChunks(route: Route, maxJsonBytes: Int): List<String> {
        val pointsBudget = (maxJsonBytes - ENVELOPE_CHARS).coerceAtLeast(POINT_CHARS * 4)
        val decimated = decimate(route.points)
        // Split into point-groups whose JSON stays under the size cap.
        val groups = ArrayList<List<GeoPoint>>()
        var current = ArrayList<GeoPoint>()
        var currentLen = 0
        for (p in decimated) {
            if (current.isNotEmpty() && currentLen + POINT_CHARS > pointsBudget) {
                groups.add(current)
                current = ArrayList()
                currentLen = 0
            }
            current.add(p)
            currentLen += POINT_CHARS
        }
        if (current.isNotEmpty()) groups.add(current)
        if (groups.isEmpty()) groups.add(emptyList())

        val total = groups.size
        return groups.mapIndexed { seq, pts ->
            val pointsJson = JSONArray()
            for (p in pts) {
                val pair = JSONArray()
                pair.put(round6(p.lat))
                pair.put(round6(p.lon))
                pointsJson.put(pair)
            }
            JSONObject().apply {
                put("type", "poly")
                put("route_id", route.routeId)
                put("seq", seq)
                put("total", total)
                put("points", pointsJson)
            }.toString()
        }
    }

    private fun decimate(points: List<GeoPoint>): List<GeoPoint> {
        if (points.size <= 2) return points
        val out = ArrayList<GeoPoint>(points.size / 2)
        out.add(points.first())
        var last = points.first()
        for (i in 1 until points.size - 1) {
            if (GeoMath.haversineM(last, points[i]) >= MIN_VERTEX_SPACING_M) {
                out.add(points[i])
                last = points[i]
            }
        }
        if (out.last() != points.last()) out.add(points.last())
        return out
    }

    private fun round6(v: Double): Double = Math.round(v * 1_000_000.0) / 1_000_000.0
}
