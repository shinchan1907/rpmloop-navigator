package io.bytenex.rpmloop.service

import io.bytenex.rpmloop.ble.BleConnectionState

/** Everything the single main screen renders. Nulls mean "not available yet"
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
    val headingDeg: Double? = null
)
