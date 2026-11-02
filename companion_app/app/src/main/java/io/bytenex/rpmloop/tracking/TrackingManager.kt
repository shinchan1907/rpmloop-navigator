package io.bytenex.rpmloop.tracking

/**
 * TODO(Phase 2): Trip tracking.
 *
 * Planned: start/stop controls live here in the app (per product decision);
 * the device is a live display only. While a trip is active the app computes
 * distance / elapsed time / average and max speed from the GPS stream and
 * sends "track" messages on the tracking characteristic
 * (see protocol_spec.json -> messages.tracking_update). Trip history
 * persistence is also Phase 2.
 *
 * Intentionally empty in Phase 1.
 */
class TrackingManager {
    // TODO(Phase 2): implement trip start/stop, live stats and history.
}
