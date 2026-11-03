package io.bytenex.rpmloop.nav

import android.location.Location
import io.bytenex.rpmloop.ble.RpmloopProtocol
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlin.math.max
import kotlin.math.roundToInt

/**
 * Turn-by-turn state machine. Owned by CompanionService and fed every GPS
 * fix; runs entirely inside the foreground service, so navigation continues
 * with the phone screen off.
 *
 * Per fix it projects the position onto the route polyline, works out the
 * current step, the distance to the next maneuver and the remaining
 * distance/ETA, and emits a spec-format "nav" message whenever the payload
 * actually changes. Behaviour thresholds (product spec):
 *  - off-route: > 60 m from the polyline for 3 consecutive fixes -> reroute
 *    through the waypoints not yet passed (20 s cooldown between reroutes);
 *  - arrival: < 25 m from the destination -> destination state once, then a
 *    final inactive nav, then the engine stops.
 */
class NavigationEngine(
    /** Called with each spec-format nav JSON payload to write to the device. */
    private val onNavPayload: (String) -> Unit,
    /** Called when a reroute is needed; argument = waypoints still to visit,
     *  destination last. The service fetches a fresh route and calls
     *  [replaceRoute] with it. */
    private val onRerouteNeeded: (remainingWaypoints: List<Waypoint>) -> Unit,
    /** Called once when the destination is reached. */
    private val onArrived: () -> Unit
) {

    companion object {
        const val OFF_ROUTE_THRESHOLD_M = 60.0
        const val OFF_ROUTE_FIXES = 3
        const val ARRIVAL_RADIUS_M = 25.0
        const val REROUTE_COOLDOWN_MS = 20_000L
        /** A waypoint counts as passed once the route position is this close
         *  to (or beyond) the leg end that reaches it. */
        const val WAYPOINT_PASSED_TOLERANCE_M = 15.0
    }

    private val _status = MutableStateFlow(NavStatus())
    val status: StateFlow<NavStatus> = _status.asStateFlow()

    @Volatile
    private var route: Route? = null

    @Volatile
    private var allWaypoints: List<Waypoint> = emptyList()

    private var offRouteCount = 0
    private var lastRerouteAtMs = 0L
    private var arrivedFired = false
    private var lastSentPayload: String? = null
    private var lastSentAtMs = 0L

    val isNavigating: Boolean get() = route != null && !arrivedFired

    val currentRoute: Route? get() = route

    /** Starts navigating [route]; [waypoints] are the user's stops +
     *  destination in visit order (aligned with Route.waypointPoints). */
    fun start(route: Route, waypoints: List<Waypoint>) {
        this.route = route
        this.allWaypoints = waypoints
        offRouteCount = 0
        arrivedFired = false
        lastSentPayload = null
        _status.value = NavStatus(
            active = true,
            message = "Navigating to ${route.destinationName}"
        )
    }

    /**
     * Replaces the route after a reroute; keeps navigating.
     * [waypointsForNewRoute] must be the waypoint list the new route was
     * built through (remaining stops + destination), so passed-waypoint
     * bookkeeping stays aligned with the new route's legs.
     */
    fun replaceRoute(newRoute: Route, waypointsForNewRoute: List<Waypoint>) {
        if (route == null) return
        route = newRoute
        allWaypoints = waypointsForNewRoute
        offRouteCount = 0
        lastSentPayload = null // force a fresh nav update on the next fix
        _status.value = _status.value.copy(
            offRoute = false,
            message = "Rerouted"
        )
    }

    /** Stops navigation; sends the inactive nav state unless [silent]. */
    fun stop(silent: Boolean = false) {
        val wasActive = route != null
        route = null
        allWaypoints = emptyList()
        offRouteCount = 0
        arrivedFired = false
        lastSentPayload = null
        _status.value = NavStatus()
        if (wasActive && !silent) {
            onNavPayload(RpmloopProtocol.buildNavInactive())
        }
    }

    /** Feed every GPS fix here (called from the service's location callback). */
    fun onFix(location: Location) {
        val r = route ?: return
        if (arrivedFired) return
        val here = GeoPoint(location.latitude, location.longitude)

        // --- arrival -----------------------------------------------------
        if (GeoMath.haversineM(here, r.destination) < ARRIVAL_RADIUS_M) {
            arrivedFired = true
            onNavPayload(
                RpmloopProtocol.buildNavUpdate(
                    maneuver = "destination",
                    distanceM = 0,
                    street = r.destinationName,
                    nextStreet = null,
                    etaMin = 0,
                    remainingKm = 0.0
                )
            )
            // End cleanly: the device leaves navigation mode.
            onNavPayload(RpmloopProtocol.buildNavInactive())
            _status.value = NavStatus(message = "Arrived at ${r.destinationName}")
            route = null
            onArrived()
            return
        }

        // --- position on route ------------------------------------------
        val proj = GeoMath.projectOntoPolyline(here, r.points, r.cumulativeM)
        val along = proj.alongM.coerceIn(0.0, r.totalDistanceM.coerceAtLeast(0.0))

        // --- off-route ----------------------------------------------------
        if (proj.crossTrackM > OFF_ROUTE_THRESHOLD_M) {
            offRouteCount++
            if (offRouteCount >= OFF_ROUTE_FIXES) {
                val now = System.currentTimeMillis()
                if (now - lastRerouteAtMs >= REROUTE_COOLDOWN_MS) {
                    lastRerouteAtMs = now
                    offRouteCount = 0
                    _status.value = _status.value.copy(
                        offRoute = true,
                        message = "Off route — rerouting…"
                    )
                    onRerouteNeeded(remainingWaypoints(along))
                }
            }
        } else {
            offRouteCount = 0
            if (_status.value.offRoute) {
                _status.value = _status.value.copy(offRoute = false, message = null)
            }
        }

        // --- current step / next maneuver ---------------------------------
        // Steps are in order; the current step is the last one whose start
        // distance is <= our position. The NEXT step's maneuver is the
        // upcoming turn, located at that step's start distance.
        var idx = 0
        for (i in r.steps.indices) {
            if (r.steps[i].startDistM <= along + 1.0) idx = i else break
        }
        val current = r.steps[idx]
        val next = r.steps.getOrNull(idx + 1)
        val targetDist = next?.startDistM ?: r.cumulativeM.last()
        val distanceToNext = max(0.0, targetDist - along)
        val maneuver = next?.maneuver ?: "destination"
        val remainingM = max(0.0, r.cumulativeM.last() - along)
        val etaMin = remainingDurationS(r, idx, along) / 60.0

        _status.value = NavStatus(
            active = true,
            currentStreet = current.name,
            nextManeuver = maneuver,
            distanceToNextM = distanceToNext.roundToInt(),
            remainingKm = remainingM / 1000.0,
            etaMin = etaMin.roundToInt(),
            offRoute = _status.value.offRoute,
            message = _status.value.message?.takeIf { _status.value.offRoute }
        )

        val payload = RpmloopProtocol.buildNavUpdate(
            maneuver = maneuver,
            distanceM = distanceToNext.roundToInt(),
            street = current.name.ifEmpty { null },
            nextStreet = next?.name?.ifEmpty { null },
            etaMin = etaMin.roundToInt(),
            remainingKm = remainingM / 1000.0
        )
        val now = System.currentTimeMillis()
        // "Whenever it changes": identical payloads are not re-sent, except
        // a 10 s keepalive so the device display never goes stale.
        if (payload != lastSentPayload || now - lastSentAtMs >= 10_000L) {
            lastSentPayload = payload
            lastSentAtMs = now
            onNavPayload(payload)
        }
    }

    /** Waypoints not yet passed at route position [alongM], destination last. */
    private fun remainingWaypoints(alongM: Double): List<Waypoint> {
        val r = route ?: return allWaypoints
        var passedLegs = 0
        for (end in r.legEndDistM) {
            if (alongM > end - WAYPOINT_PASSED_TOLERANCE_M) passedLegs++ else break
        }
        if (passedLegs >= allWaypoints.size) return allWaypoints.takeLast(1)
        return allWaypoints.drop(passedLegs)
    }

    /** Remaining travel time: prorate the current step, sum the later ones. */
    private fun remainingDurationS(r: Route, currentIdx: Int, along: Double): Double {
        var total = 0.0
        for (i in currentIdx until r.steps.size) {
            val step = r.steps[i]
            total += if (i == currentIdx && step.distanceM > 0.0) {
                val frac = ((along - step.startDistM) / step.distanceM).coerceIn(0.0, 1.0)
                step.durationS * (1.0 - frac)
            } else {
                step.durationS
            }
        }
        return total
    }
}
