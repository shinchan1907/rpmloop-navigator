package io.bytenex.rpmloop.ble

import android.location.Location
import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

/**
 * RPMLOOP BLE Protocol v3.0.0 — constants and JSON codecs.
 *
 * Source of truth: protocol/protocol_spec.json and protocol/fixtures/*.json
 * in the firmware repo. Do not rename fields or UUIDs here without updating
 * the spec; the ESP32 firmware parses these payloads exactly as written.
 */
object RpmloopProtocol {

    const val DEVICE_NAME = "RPMLOOP"
    const val PROTOCOL_VERSION = "3.0.0"
    const val APP_NAME = "RPMLOOP Companion"

    /** Spec: handshake characteristic declares max_mtu 512. The JSON payloads
     *  (~110–150 bytes) do not fit the default 23-byte MTU, so the app must
     *  request this MTU right after connecting. */
    const val REQUESTED_MTU = 512

    val SERVICE_UUID: UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    val HANDSHAKE_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
    val POSITION_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a9")
    val NAV_STATE_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26aa")
    val POLYLINE_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26ab")
    val MEDIA_STATE_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26ac")
    val MEDIA_ART_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26ad")
    val TRACKING_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26ae")
    val DEVICE_CMD_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26af")

    // ---- Protocol v3.1 extension: file transfer (offline tiles) -------------
    // Two extra characteristics on the SAME service, used only to push tile
    // files from the phone to the device's SD card. The v3.0.0 session
    // (hello/hello_ack above) is unchanged; v3.1 transfer runs inside it.
    val FILE_DATA_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26b0")
    val FILE_CTRL_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26b1")

    /** Client Characteristic Configuration descriptor (for enabling notifications). */
    val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    /** Hello message — byte-for-byte the shape of fixtures/hello_request.json. */
    fun buildHello(): String = JSONObject().apply {
        put("type", "hello")
        put("version", PROTOCOL_VERSION)
        put("app_name", APP_NAME)
        put("platform", "android")
        put("capabilities", JSONArray().apply {
            put("gps")
            put("nav")
            put("media")
            put("tracking")
        })
    }.toString()

    data class HelloAck(
        val version: String,
        val deviceName: String,
        val fwVersion: String,
        val hasSd: Boolean,
        val displayWidth: Int?,
        val displayHeight: Int?
    )

    /** Parses a hello_ack notification; returns null if it is not one. */
    fun parseHelloAck(json: String): HelloAck? = try {
        val o = JSONObject(json)
        if (o.optString("type") != "hello_ack") {
            null
        } else {
            val display = o.optJSONObject("display")
            HelloAck(
                version = o.optString("version"),
                deviceName = o.optString("device_name"),
                fwVersion = o.optString("fw_version"),
                hasSd = o.optBoolean("has_sd"),
                displayWidth = display?.optInt("width"),
                displayHeight = display?.optInt("height")
            )
        }
    } catch (e: Exception) {
        null
    }

    /**
     * Position update — the shape of fixtures/position_update.json:
     * {"type":"pos","lat","lon","speed_kmh","heading","accuracy_m","ts"}.
     *
     * Fields the current fix does not provide (speed / bearing / accuracy)
     * are omitted rather than fabricated; the firmware applies its own
     * defaults for missing keys. "ts" is GPS fix time in epoch seconds.
     * The spec defines no altitude field, so none is sent.
     */
    fun buildPosition(location: Location): String = JSONObject().apply {
        put("type", "pos")
        put("lat", location.latitude)
        put("lon", location.longitude)
        if (location.hasSpeed()) {
            put("speed_kmh", location.speed.toDouble() * 3.6) // m/s -> km/h
        }
        if (location.hasBearing()) {
            put("heading", location.bearing.toDouble())
        }
        if (location.hasAccuracy()) {
            put("accuracy_m", location.accuracy.toDouble())
        }
        put("ts", location.time / 1000)
    }.toString()

    // ---- nav_state (spec: messages.nav_state_update) -------------------------

    /**
     * Active navigation update — byte-for-byte the shape of
     * fixtures/nav_update.json. [maneuver] must be one of the spec's
     * maneuver_types (see OsrmClient.mapManeuver for the OSRM mapping).
     * [nextStreet] is omitted when unknown rather than invented.
     */
    fun buildNavUpdate(
        maneuver: String,
        distanceM: Int,
        street: String?,
        nextStreet: String?,
        etaMin: Int,
        remainingKm: Double
    ): String = JSONObject().apply {
        put("type", "nav")
        put("active", true)
        put("maneuver", maneuver)
        put("distance_m", distanceM)
        if (!street.isNullOrEmpty()) put("street", street)
        if (!nextStreet.isNullOrEmpty()) put("next_street", nextStreet)
        put("eta_min", etaMin)
        put("remaining_km", Math.round(remainingKm * 10.0) / 10.0)
    }.toString()

    /** Navigation ended — the device should leave navigation mode. */
    fun buildNavInactive(): String =
        JSONObject().put("type", "nav").put("active", false).toString()

    /** New route id, shaped like the spec example ("r001"). */
    fun newRouteId(): String = "r" + (System.currentTimeMillis() % 100000L)

    // ---- file transfer control messages (protocol v3.1) ----------------------

    fun buildFileBegin(path: String, size: Long): String = JSONObject().apply {
        put("type", "file_begin")
        put("path", path)
        put("size", size)
    }.toString()

    fun buildFileEnd(size: Long): String = JSONObject().apply {
        put("type", "file_end")
        put("size", size)
    }.toString()

    /** Parsed device reply on the FILE_CTRL characteristic. */
    sealed class FileCtrlMessage {
        data class Ready(val offset: Long) : FileCtrlMessage()
        data class Ack(val seq: Int) : FileCtrlMessage()
        object Done : FileCtrlMessage()
        data class Error(val reason: String) : FileCtrlMessage()
        object Unknown : FileCtrlMessage()
    }

    /** Parses any JSON arriving on FILE_CTRL; never throws. */
    fun parseFileCtrl(json: String): FileCtrlMessage = try {
        val o = JSONObject(json)
        when (o.optString("type")) {
            "file_ready" -> FileCtrlMessage.Ready(o.optLong("offset", 0L))
            "file_ack" -> FileCtrlMessage.Ack(o.optInt("seq", -1))
            "file_done" -> FileCtrlMessage.Done
            "file_error" -> FileCtrlMessage.Error(
                o.optString("reason").ifEmpty { o.optString("message") }
                    .ifEmpty { "device reported file_error" }
            )
            else -> FileCtrlMessage.Unknown
        }
    } catch (e: Exception) {
        FileCtrlMessage.Unknown
    }
}
