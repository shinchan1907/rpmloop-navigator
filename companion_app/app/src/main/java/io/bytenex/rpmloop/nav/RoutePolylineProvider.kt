package io.bytenex.rpmloop.nav

/**
 * TODO(Phase 2): Route polyline fetching and transfer.
 *
 * Planned: fetch the active route geometry (Directions API or OSRM), assign
 * a route_id, and stream it to the device as chunked, sequenced "poly"
 * messages on the polyline characteristic
 * (see protocol_spec.json -> messages.polyline_chunk). The device draws the
 * route over its offline SD tile pack.
 *
 * Intentionally empty in Phase 1.
 */
class RoutePolylineProvider {
    // TODO(Phase 2): implement route fetch + chunked polyline transfer.
}
