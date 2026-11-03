package io.bytenex.rpmloop.nav

/** A user-chosen stop or destination, in visit order. */
data class Waypoint(val name: String, val point: GeoPoint)

/** One Nominatim search hit. */
data class SearchResult(val name: String, val point: GeoPoint)

/**
 * One OSRM step, re-expressed against the route's full polyline.
 * The step's maneuver happens at [startDistM] (the END of this step is the
 * next step's maneuver point — see OsrmClient for the semantics).
 */
data class RouteStep(
    /** Protocol maneuver string (spec maneuver_types), already mapped. */
    val maneuver: String,
    /** Raw OSRM maneuver type, for debugging / future use. */
    val osrmType: String,
    /** Road name for this step ("" when OSRM gives none). */
    val name: String,
    val distanceM: Double,
    val durationS: Double,
    /** Vertex index into Route.points where this step's geometry starts. */
    val startIndex: Int,
    /** Distance along the route at [startIndex], metres. */
    val startDistM: Double
)

/** A fully parsed OSRM route, ready for the NavigationEngine. */
class Route(
    val routeId: String,
    val points: List<GeoPoint>,
    /** Cumulative metres at each vertex of [points]. */
    val cumulativeM: List<Double>,
    val steps: List<RouteStep>,
    /** waypointPoints[i] is reached at legEndDistM[i] along the route. */
    val waypointPoints: List<GeoPoint>,
    val legEndDistM: List<Double>,
    val totalDistanceM: Double,
    val totalDurationS: Double,
    val destinationName: String
) {
    val destination: GeoPoint
        get() = waypointPoints.lastOrNull() ?: points.last()
}

/** Live navigation snapshot for the UI / foreground notification. */
data class NavStatus(
    val active: Boolean = false,
    /** Current OSRM step's road name ("" if unknown). */
    val currentStreet: String = "",
    /** Protocol maneuver of the upcoming turn ("destination" on final approach). */
    val nextManeuver: String? = null,
    val distanceToNextM: Int = 0,
    val remainingKm: Double = 0.0,
    val etaMin: Int = 0,
    val offRoute: Boolean = false,
    /** One-line human message: errors, "Off route — rerouting…", "Arrived". */
    val message: String? = null
)
