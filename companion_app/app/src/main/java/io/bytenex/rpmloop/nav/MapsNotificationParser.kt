package io.bytenex.rpmloop.nav

/**
 * TODO(Phase 2): Google Maps navigation parsing.
 *
 * Planned: a NotificationListenerService reads Google Maps navigation
 * notifications, extracts maneuver / distance / street / ETA, and sends them
 * to the device as "nav" messages on the nav_state characteristic
 * (see protocol_spec.json -> messages.nav_state_update).
 *
 * Intentionally empty in Phase 1 — no fake navigation data is ever shown.
 */
class MapsNotificationParser {
    // TODO(Phase 2): implement notification parsing + nav_state sending.
}
