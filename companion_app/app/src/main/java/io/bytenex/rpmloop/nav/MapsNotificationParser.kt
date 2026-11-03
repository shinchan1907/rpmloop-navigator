package io.bytenex.rpmloop.nav

/**
 * NOT IMPLEMENTED — by design, not by omission.
 *
 * Phase 1 planned to scrape Google Maps navigation notifications for
 * turn-by-turn data. That approach was superseded: navigation is now
 * computed in-app by [NavigationEngine] from OSRM routes (see
 * [OsrmClient], [GeocodingClient]), which works with the screen off, needs
 * no notification-access permission, and produces the exact spec-format
 * "nav" messages directly.
 *
 * This class is kept as a marker in case a Maps-notification fallback is
 * ever wanted. It contains no code and produces no data.
 */
class MapsNotificationParser {
    // Intentionally empty — see class doc.
}
