package io.bytenex.rpmloop.nav

import io.bytenex.rpmloop.ble.RpmloopProtocol
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.util.Locale

/**
 * Routing against the public OSRM demo server (driving profile).
 *
 * Response semantics used here: with steps=true, each leg is a list of
 * steps; a step's maneuver sits at the START of that step's geometry and
 * the step then describes travelling to the next maneuver. The route's full
 * polyline is therefore the concatenation of all step geometries (dropping
 * the duplicated joint vertex between consecutive steps), which also gives
 * every step an exact vertex index / distance along the full route.
 */
object OsrmClient {

    private const val USER_AGENT = "RPMLOOP Companion/2.0 (io.bytenex.rpmloop; Android)"

    /**
     * Fetches a route through [points] (start = points[0], then intermediate
     * stops in order, destination = points.last()).
     *
     * [waypointNames] is aligned with points[1..] (used for the destination
     * label). Throws on any network / parse failure.
     */
    suspend fun fetchRoute(
        points: List<GeoPoint>,
        waypointNames: List<String>
    ): Route = withContext(Dispatchers.IO) {
        require(points.size >= 2) { "Need at least a start and a destination" }
        val coords = points.joinToString(";") {
            String.format(Locale.US, "%f,%f", it.lon, it.lat)
        }
        val url = URL(
            "https://router.project-osrm.org/route/v1/driving/$coords" +
                "?overview=full&geometries=geojson&steps=true&alternatives=false"
        )
        val conn = (url.openConnection() as HttpURLConnection).apply {
            requestMethod = "GET"
            connectTimeout = 10_000
            readTimeout = 20_000
            setRequestProperty("User-Agent", USER_AGENT)
            setRequestProperty("Accept", "application/json")
        }
        try {
            val code = conn.responseCode
            if (code != HttpURLConnection.HTTP_OK) {
                throw IllegalStateException("Routing failed (HTTP $code)")
            }
            val body = conn.inputStream.bufferedReader().use { it.readText() }
            parseRoute(body, points, waypointNames)
        } finally {
            conn.disconnect()
        }
    }

    private fun parseRoute(
        body: String,
        requestedPoints: List<GeoPoint>,
        waypointNames: List<String>
    ): Route {
        val root = JSONObject(body)
        if (root.optString("code") != "Ok") {
            throw IllegalStateException("Routing failed: ${root.optString("message", "no route")}")
        }
        val routeObj = root.optJSONArray("routes")?.optJSONObject(0)
            ?: throw IllegalStateException("Routing failed: empty response")
        val legs = routeObj.optJSONArray("legs")
            ?: throw IllegalStateException("Routing failed: no legs")

        val fullPoints = ArrayList<GeoPoint>()
        val steps = ArrayList<RouteStep>()
        // Waypoints reached at each leg end: requestedPoints[1..] in order.
        val waypointPoints = requestedPoints.drop(1)

        for (legIdx in 0 until legs.length()) {
            val leg = legs.optJSONObject(legIdx) ?: continue
            val legSteps = leg.optJSONArray("steps") ?: continue
            for (s in 0 until legSteps.length()) {
                val stepObj = legSteps.optJSONObject(s) ?: continue
                val geometry = stepObj.optJSONObject("geometry")
                val coords = geometry?.optJSONArray("coordinates")
                val startIndex = fullPoints.size
                if (coords != null) {
                    for (c in 0 until coords.length()) {
                        val pair = coords.optJSONArray(c) ?: continue
                        // GeoJSON order is [lon, lat].
                        val p = GeoPoint(pair.optDouble(1), pair.optDouble(0))
                        if (fullPoints.isNotEmpty() && fullPoints.last() == p) continue
                        fullPoints.add(p)
                    }
                }
                if (fullPoints.isEmpty()) continue
                val maneuverObj = stepObj.optJSONObject("maneuver")
                val osrmType = maneuverObj?.optString("type") ?: "continue"
                val modifier = maneuverObj?.optString("modifier")?.takeIf { it.isNotEmpty() }
                steps.add(
                    RouteStep(
                        maneuver = mapManeuver(osrmType, modifier),
                        osrmType = osrmType,
                        name = stepObj.optString("name"),
                        distanceM = stepObj.optDouble("distance", 0.0),
                        durationS = stepObj.optDouble("duration", 0.0),
                        startIndex = startIndex,
                        startDistM = 0.0 // filled in below, once cumulative exists
                    )
                )
            }
        }
        if (fullPoints.size < 2 || steps.isEmpty()) {
            throw IllegalStateException("Routing failed: route has no geometry")
        }

        val cumulative = GeoMath.cumulativeDistances(fullPoints)
        val fixedSteps = steps.map { st ->
            RouteStep(
                maneuver = st.maneuver,
                osrmType = st.osrmType,
                name = st.name,
                distanceM = st.distanceM,
                durationS = st.durationS,
                startIndex = st.startIndex,
                startDistM = cumulative.getOrElse(st.startIndex) { cumulative.last() }
            )
        }
        // Leg end distance = start distance of the first step of the NEXT leg.
        // Recompute per leg by counting steps per leg again from the JSON is
        // overkill; instead, leg boundaries are the cumulative distances at
        // the vertices nearest each requested waypoint.
        val fixedLegEnds = waypointPoints.map { wp ->
            var bestIdx = 0
            var best = Double.MAX_VALUE
            for (i in fullPoints.indices) {
                val d = GeoMath.haversineM(wp, fullPoints[i])
                if (d < best) {
                    best = d
                    bestIdx = i
                }
            }
            cumulative[bestIdx]
        }

        return Route(
            routeId = RpmloopProtocol.newRouteId(),
            points = fullPoints,
            cumulativeM = cumulative,
            steps = fixedSteps,
            waypointPoints = waypointPoints,
            legEndDistM = fixedLegEnds,
            totalDistanceM = routeObj.optDouble("distance", cumulative.last()),
            totalDurationS = routeObj.optDouble("duration", 0.0),
            destinationName = waypointNames.lastOrNull() ?: "Destination"
        )
    }

    /**
     * Maps an OSRM maneuver (type + modifier) onto the protocol's
     * maneuver_types list (protocol_spec.json). Anything unrecognised
     * degrades to "straight" — the device still shows distance/street.
     */
    fun mapManeuver(type: String, modifier: String?): String {
        val mod = modifier ?: ""
        return when (type) {
            "arrive" -> when (mod) {
                "left" -> "destination_left"
                "right" -> "destination_right"
                else -> "destination"
            }
            "depart" -> "straight"
            "turn", "end of road" -> when (mod) {
                "sharp left" -> "sharp_left"
                "sharp right" -> "sharp_right"
                "left" -> "turn_left"
                "right" -> "turn_right"
                "slight left" -> "slight_left"
                "slight right" -> "slight_right"
                // OSRM gives no side for a plain "uturn" modifier.
                "uturn" -> "u_turn_left"
                else -> "straight"
            }
            "continue" -> "straight"
            "merge" -> "merge"
            "fork" -> if (mod.contains("left")) "fork_left" else "fork_right"
            "on ramp", "off ramp" ->
                if (mod.contains("left")) "ramp_left" else "ramp_right"
            "roundabout", "rotary", "roundabout turn", "exit roundabout" ->
                if (mod.contains("right")) "roundabout_right" else "roundabout_left"
            else -> "straight"
        }
    }
}
