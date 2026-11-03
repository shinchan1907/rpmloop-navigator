package io.bytenex.rpmloop.service

import io.bytenex.rpmloop.ble.BleConnectionState

/** One row in the waypoint list UI (intermediate stops, destination last). */
data class WaypointRow(
    val name: String,
    val isDestination: Boolean
)

/** Everything about the Navigation section of the main screen. */
data class NavUiState(
    /** Stops + destination in visit order; destination (if any) is last. */
    val waypoints: List<WaypointRow> = emptyList(),
    /** e.g. "12.4 km · 18 min · 3 stops" after a successful plan. */
    val routeSummary: String? = null,
    val navigating: Boolean = false,
    /** e.g. "Turn right · 150 m" while navigating. */
    val nextTurn: String? = null,
    /** e.g. "5.3 km · 12 min remaining". */
    val remaining: String? = null,
    /** Errors / off-route / arrival messages; null = nothing to report. */
    val message: String? = null,
    val planning: Boolean = false
)

/** Everything about the Offline tiles section of the main screen. */
data class TilesUiState(
    val hasApiKey: Boolean = false,
    val downloadedCount: Int = 0,
    val downloading: Boolean = false,
    /** "Route tiles" / "Nearby tiles" while a job runs. */
    val downloadJob: String? = null,
    val downloadDone: Int = 0,
    val downloadTotal: Int = 0,
    val downloadFailed: Int = 0,
    val transferring: Boolean = false,
    val transferFilesDone: Int = 0,
    val transferFilesTotal: Int = 0,
    val transferCurrentFile: String? = null,
    val transferFilePct: Int = 0,
    val transferKBps: Int = 0,
    val message: String? = null
)

/** Everything the main screen renders. Nulls mean "not available yet"
 *  and are shown as honest placeholders, never invented values. */
data class AppUiState(
    val connection: BleConnectionState = BleConnectionState.DISCONNECTED,
    val deviceName: String? = null,
    /** null = handshake not attempted yet. */
    val handshake: String? = null,
    val detail: String? = null,
    val latitude: Double? = null,
    val longitude: Double? = null,
    val speedKmh: Double? = null,
    val headingDeg: Double? = null,
    val nav: NavUiState = NavUiState(),
    val tiles: TilesUiState = TilesUiState()
)
