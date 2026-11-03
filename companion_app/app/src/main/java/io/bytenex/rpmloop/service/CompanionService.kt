package io.bytenex.rpmloop.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.location.Location
import android.os.Binder
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import io.bytenex.rpmloop.MainActivity
import io.bytenex.rpmloop.R
import io.bytenex.rpmloop.ble.BleConnectionState
import io.bytenex.rpmloop.ble.BleManager
import io.bytenex.rpmloop.location.LocationProvider
import io.bytenex.rpmloop.nav.GeoPoint
import io.bytenex.rpmloop.nav.NavigationEngine
import io.bytenex.rpmloop.nav.OsrmClient
import io.bytenex.rpmloop.nav.Route
import io.bytenex.rpmloop.nav.RoutePolylineProvider
import io.bytenex.rpmloop.nav.Waypoint
import io.bytenex.rpmloop.tiles.TileCoord
import io.bytenex.rpmloop.tiles.TileDownloader
import io.bytenex.rpmloop.tiles.TilePlanner
import io.bytenex.rpmloop.tiles.TileStore
import io.bytenex.rpmloop.tiles.TileTransferManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.util.Locale

/**
 * Foreground service (location type) that owns the BLE session and the GPS
 * stream so both keep running with the screen off. The persistent
 * notification always shows the current connection state.
 */
class CompanionService : Service() {

    companion object {
        const val ACTION_START = "io.bytenex.rpmloop.action.START"
        const val ACTION_STOP = "io.bytenex.rpmloop.action.STOP"

        private const val NOTIFICATION_ID = 1001
        private const val CHANNEL_ID = "rpmloop_connection"

        /** True while the service engine is running; lets the Activity
         *  re-attach after rotation or process return. */
        @Volatile
        var isRunning: Boolean = false
            private set
    }

    inner class LocalBinder : Binder() {
        fun getService(): CompanionService = this@CompanionService
    }

    private val binder = LocalBinder()

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)

    private lateinit var bleManager: BleManager
    private lateinit var locationProvider: LocationProvider

    // --- navigation ---------------------------------------------------------
    private lateinit var navEngine: NavigationEngine
    private val stops = mutableListOf<Waypoint>()
    private var destination: Waypoint? = null
    private var plannedRoute: Route? = null
    private var routeSummaryText: String? = null
    private var navServiceMessage: String? = null
    private var planningRoute = false

    @Volatile
    private var lastLocation: Location? = null

    // --- offline tiles --------------------------------------------------------
    private lateinit var tileStore: TileStore
    private lateinit var tileDownloader: TileDownloader
    private lateinit var transferManager: TileTransferManager

    private val _state = MutableStateFlow(AppUiState())
    val state: StateFlow<AppUiState> = _state.asStateFlow()

    @Volatile
    private var started = false

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        bleManager = BleManager(applicationContext)
        locationProvider = LocationProvider(applicationContext) { location ->
            onNewLocation(location)
        }
        tileStore = TileStore(applicationContext)
        tileDownloader = TileDownloader(tileStore)
        transferManager = TileTransferManager(bleManager, tileStore)
        navEngine = NavigationEngine(
            onNavPayload = { json -> bleManager.sendNav(json) },
            onRerouteNeeded = { remaining -> handleReroute(remaining) },
            onArrived = { refreshNavUi() }
        )
        _state.update {
            it.copy(
                tiles = it.tiles.copy(
                    hasApiKey = tileStore.hasApiKey(),
                    downloadedCount = tileStore.downloadedCount()
                )
            )
        }
        scope.launch {
            navEngine.status.collect {
                refreshNavUi()
                if (started) refreshNotification()
            }
        }
        scope.launch {
            bleManager.status.collect { ble ->
                _state.update {
                    it.copy(
                        connection = ble.state,
                        deviceName = ble.deviceName,
                        handshake = ble.handshake,
                        detail = ble.detail
                    )
                }
                if (started) refreshNotification()
            }
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopEverything()
            return START_NOT_STICKY
        }
        // ACTION_START (or a sticky restart with a null intent)
        promoteToForeground()
        startEngine()
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onDestroy() {
        if (started) {
            navEngine.stop(silent = true)
            tileDownloader.cancel()
            transferManager.cancel()
            locationProvider.stop()
            bleManager.stop()
        }
        started = false
        isRunning = false
        scope.cancel()
        super.onDestroy()
    }

    /** Stops scanning, BLE and GPS, removes the notification and the service. */
    fun stopEverything() {
        if (!started) {
            stopSelf()
            return
        }
        started = false
        isRunning = false
        // Silent: the BLE link is going down anyway, and the device ends its
        // nav session with the connection.
        navEngine.stop(silent = true)
        stops.clear()
        destination = null
        plannedRoute = null
        routeSummaryText = null
        navServiceMessage = null
        planningRoute = false
        tileDownloader.cancel()
        transferManager.cancel()
        locationProvider.stop()
        bleManager.stop()
        _state.value = AppUiState(
            tiles = TilesUiState(
                hasApiKey = tileStore.hasApiKey(),
                downloadedCount = tileStore.downloadedCount()
            )
        )
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private fun startEngine() {
        if (started) return
        started = true
        isRunning = true
        bleManager.start()
        locationProvider.start()
    }

    private fun onNewLocation(location: android.location.Location) {
        lastLocation = location
        _state.update {
            it.copy(
                latitude = location.latitude,
                longitude = location.longitude,
                speedKmh = if (location.hasSpeed()) {
                    location.speed.toDouble() * 3.6
                } else {
                    null
                },
                headingDeg = if (location.hasBearing()) {
                    location.bearing.toDouble()
                } else {
                    null
                }
            )
        }
        bleManager.sendPosition(location)
        navEngine.onFix(location)
    }

    // ---- navigation: waypoints & planning --------------------------------------

    /** Adds an intermediate stop ("break point") to the visit list. */
    fun addStop(name: String, point: GeoPoint) {
        invalidateRouteIfNavigating()
        stops.add(Waypoint(name, point))
        plannedRoute = null
        routeSummaryText = null
        navServiceMessage = null
        refreshNavUi()
    }

    /** Sets (or replaces) the single destination. */
    fun setDestination(name: String, point: GeoPoint) {
        invalidateRouteIfNavigating()
        destination = Waypoint(name, point)
        plannedRoute = null
        routeSummaryText = null
        navServiceMessage = null
        refreshNavUi()
    }

    /** [index] addresses the combined UI list: stops first, destination last. */
    fun removeWaypoint(index: Int) {
        invalidateRouteIfNavigating()
        if (index in stops.indices) {
            stops.removeAt(index)
        } else if (index == stops.size && destination != null) {
            destination = null
        } else {
            return
        }
        plannedRoute = null
        routeSummaryText = null
        refreshNavUi()
    }

    private fun invalidateRouteIfNavigating() {
        if (navEngine.isNavigating) {
            navEngine.stop()
            navServiceMessage = "Route changed — navigation stopped"
        }
    }

    /** Fetches the OSRM route from the current GPS position through the
     *  stops to the destination and sends the polyline to the device. */
    fun planRoute() {
        val dest = destination
        if (dest == null) {
            navServiceMessage = "Add a destination first"
            refreshNavUi()
            return
        }
        val origin = lastLocation
        if (origin == null) {
            navServiceMessage = "Waiting for a GPS fix to plan from…"
            refreshNavUi()
            return
        }
        if (planningRoute) return
        planningRoute = true
        refreshNavUi()
        val orderedStops = stops.toList()
        scope.launch {
            try {
                val points = ArrayList<GeoPoint>()
                points.add(GeoPoint(origin.latitude, origin.longitude))
                points.addAll(orderedStops.map { it.point })
                points.add(dest.point)
                val names = orderedStops.map { it.name } + dest.name
                val route = OsrmClient.fetchRoute(points, names)
                plannedRoute = route
                routeSummaryText = String.format(
                    Locale.US, "%.1f km · %d min · %s",
                    route.totalDistanceM / 1000.0,
                    (route.totalDurationS / 60.0).toInt(),
                    route.destinationName
                )
                val sent = bleManager.sendPolyline(RoutePolylineProvider.buildChunks(route, bleManager.attPayloadSize()))
                navServiceMessage = if (sent) {
                    null
                } else {
                    "Route planned — connect to the device to send the route line"
                }
            } catch (e: Exception) {
                plannedRoute = null
                routeSummaryText = null
                navServiceMessage = "Routing failed: ${e.message ?: "unknown error"}"
            } finally {
                planningRoute = false
                refreshNavUi()
            }
        }
    }

    fun startNavigation() {
        val route = plannedRoute
        if (route == null) {
            navServiceMessage = "Plan a route first"
            refreshNavUi()
            return
        }
        // (Re)send the polyline so the device draws this exact route even if
        // it connected after planning.
        bleManager.sendPolyline(RoutePolylineProvider.buildChunks(route, bleManager.attPayloadSize()))
        val waypoints = stops.toList() + listOfNotNull(destination)
        navServiceMessage = null
        navEngine.start(route, waypoints)
        refreshNavUi()
    }

    fun stopNavigation() {
        navEngine.stop()
        navServiceMessage = null
        refreshNavUi()
    }

    private fun handleReroute(remaining: List<Waypoint>) {
        val origin = lastLocation ?: return
        scope.launch {
            try {
                val points = ArrayList<GeoPoint>()
                points.add(GeoPoint(origin.latitude, origin.longitude))
                points.addAll(remaining.map { it.point })
                val route = OsrmClient.fetchRoute(points, remaining.map { it.name })
                navEngine.replaceRoute(route, remaining)
                bleManager.sendPolyline(RoutePolylineProvider.buildChunks(route, bleManager.attPayloadSize()))
            } catch (e: Exception) {
                navServiceMessage = "Reroute failed: ${e.message ?: "unknown error"}"
                refreshNavUi()
            }
        }
    }

    private fun refreshNavUi() {
        val es = navEngine.status.value
        val nextManeuver = es.nextManeuver
        val rows = stops.map { WaypointRow(it.name, isDestination = false) } +
            listOfNotNull(destination?.let { WaypointRow(it.name, isDestination = true) })
        _state.update {
            it.copy(
                nav = NavUiState(
                    waypoints = rows,
                    routeSummary = routeSummaryText,
                    navigating = es.active,
                    nextTurn = if (es.active && nextManeuver != null) {
                        "${maneuverLabel(nextManeuver)} · ${formatDistance(es.distanceToNextM)}"
                    } else {
                        null
                    },
                    remaining = if (es.active) {
                        String.format(
                            Locale.US, "%.1f km · %d min remaining",
                            es.remainingKm, es.etaMin
                        )
                    } else {
                        null
                    },
                    message = es.message ?: navServiceMessage,
                    planning = planningRoute
                )
            )
        }
    }

    private fun maneuverLabel(maneuver: String): String = when (maneuver) {
        "turn_left" -> "Turn left"
        "turn_right" -> "Turn right"
        "slight_left" -> "Bear left"
        "slight_right" -> "Bear right"
        "sharp_left" -> "Sharp left"
        "sharp_right" -> "Sharp right"
        "u_turn_left", "u_turn_right" -> "Make a U-turn"
        "roundabout_left", "roundabout_right" -> "At the roundabout"
        "merge" -> "Merge"
        "fork_left" -> "Keep left at the fork"
        "fork_right" -> "Keep right at the fork"
        "ramp_left" -> "Take the ramp (left)"
        "ramp_right" -> "Take the ramp (right)"
        "destination", "destination_left", "destination_right" -> "Arriving"
        else -> "Continue straight"
    }

    private fun formatDistance(m: Int): String = when {
        m < 1000 -> "$m m"
        else -> String.format(Locale.US, "%.1f km", m / 1000.0)
    }

    // ---- offline tiles ----------------------------------------------------------

    fun setTileApiKey(key: String?) {
        tileStore.setApiKey(key)
        _state.update {
            it.copy(tiles = it.tiles.copy(hasApiKey = tileStore.hasApiKey()))
        }
    }

    /** Tile count + estimated bytes for the route corridor, or null when no
     *  route is planned. Computation only — nothing downloads yet. */
    fun estimateRouteTiles(): Pair<Int, Long>? {
        val route = plannedRoute ?: navEngine.currentRoute ?: return null
        val tiles = TilePlanner.corridorTiles(route.points)
        return tiles.size to TilePlanner.estimateTotalBytes(tiles)
    }

    /** Tile count + estimated bytes for 5 km around the current GPS fix. */
    fun estimateNearbyTiles(): Pair<Int, Long>? {
        val origin = lastLocation ?: return null
        val tiles = TilePlanner.nearbyTiles(GeoPoint(origin.latitude, origin.longitude))
        return tiles.size to TilePlanner.estimateTotalBytes(tiles)
    }

    fun startRouteTileDownload() {
        val route = plannedRoute ?: navEngine.currentRoute
        if (route == null) {
            setTilesMessage("Plan a route first")
            return
        }
        startTileJob("Route tiles", TilePlanner.corridorTiles(route.points))
    }

    fun startNearbyTileDownload() {
        val origin = lastLocation
        if (origin == null) {
            setTilesMessage("Waiting for a GPS fix…")
            return
        }
        startTileJob(
            "Nearby tiles (5 km)",
            TilePlanner.nearbyTiles(GeoPoint(origin.latitude, origin.longitude))
        )
    }

    private fun startTileJob(jobName: String, tiles: List<TileCoord>) {
        val key = tileStore.apiKey()
        if (key == null) {
            setTilesMessage("Set a Google Maps Tile API key first (Tiles settings below)")
            return
        }
        if (tileDownloader.isRunning) return
        _state.update {
            it.copy(
                tiles = it.tiles.copy(
                    downloading = true,
                    downloadJob = jobName,
                    downloadDone = 0,
                    downloadTotal = tiles.size,
                    downloadFailed = 0,
                    message = null
                )
            )
        }
        tileDownloader.start(tiles, key, object : TileDownloader.Listener {
            override fun onProgress(done: Int, total: Int, failed: Int) {
                _state.update {
                    it.copy(
                        tiles = it.tiles.copy(
                            downloadDone = done,
                            downloadTotal = total,
                            downloadFailed = failed
                        )
                    )
                }
            }

            override fun onFinished(cancelled: Boolean, failed: Int, error: String?) {
                _state.update {
                    it.copy(
                        tiles = it.tiles.copy(
                            downloading = false,
                            downloadJob = null,
                            downloadedCount = tileStore.downloadedCount(),
                            message = error ?: when {
                                cancelled -> "$jobName download cancelled"
                                failed > 0 -> "$jobName: finished with $failed failed tiles"
                                else -> "$jobName: download complete"
                            }
                        )
                    )
                }
            }
        })
    }

    fun cancelTileDownload() {
        tileDownloader.cancel()
    }

    fun sendTilesToDevice() {
        if (transferManager.isRunning) return
        if (!bleManager.fileTransferReady()) {
            setTilesMessage(
                "Not ready: connect to the device first (tile transfer needs " +
                    "a completed handshake and the v3.1 file characteristics)"
            )
            return
        }
        scope.launch(Dispatchers.IO) {
            _state.update {
                it.copy(
                    tiles = it.tiles.copy(
                        transferring = true,
                        transferFilesDone = 0,
                        transferFilesTotal = 0,
                        transferFilePct = 0,
                        message = null
                    )
                )
            }
            transferManager.sendPending { progress ->
                _state.update {
                    it.copy(
                        tiles = it.tiles.copy(
                            transferring = progress.running,
                            transferFilesDone = progress.filesDone,
                            transferFilesTotal = progress.filesTotal,
                            transferCurrentFile = progress.currentFile,
                            transferFilePct = progress.currentFilePct,
                            transferKBps = progress.approxKBps,
                            message = if (progress.finished) progress.message else it.tiles.message
                        )
                    )
                }
            }
            _state.update { it.copy(tiles = it.tiles.copy(transferring = false)) }
        }
    }

    fun cancelTileTransfer() {
        transferManager.cancel()
    }

    private fun setTilesMessage(message: String) {
        _state.update { it.copy(tiles = it.tiles.copy(message = message)) }
    }

    // ---- foreground notification ----------------------------------------------

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            "Connection status",
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Persistent notification while RPMLOOP Companion is running"
        }
        val manager = getSystemService(NotificationManager::class.java)
        manager?.createNotificationChannel(channel)
    }

    private fun connectionText(state: AppUiState): String {
        // While navigating, the notification carries the next instruction —
        // more useful at a glance than the raw connection state.
        if (state.nav.navigating) {
            val turn = state.nav.nextTurn
            return if (turn != null) "Navigating · $turn" else "Navigating"
        }
        return when (state.connection) {
            BleConnectionState.DISCONNECTED -> "Disconnected"
            BleConnectionState.SCANNING -> "Scanning for RPMLOOP…"
            BleConnectionState.CONNECTING -> "Connecting to RPMLOOP…"
            BleConnectionState.CONNECTED ->
                state.handshake?.let { "Connected · $it" } ?: "Connected to RPMLOOP"
        }
    }

    private fun buildNotification(state: AppUiState): Notification {
        val contentIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val stopIntent = PendingIntent.getService(
            this,
            1,
            Intent(this, CompanionService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_nav_arrow)
            .setContentTitle("RPMLOOP Companion")
            .setContentText(connectionText(state))
            .setOngoing(true)
            .setContentIntent(contentIntent)
            .addAction(0, "Stop", stopIntent)
            .build()
    }

    private fun promoteToForeground() {
        val notification = buildNotification(_state.value)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun refreshNotification() {
        val manager = getSystemService(NotificationManager::class.java)
        manager?.notify(NOTIFICATION_ID, buildNotification(_state.value))
    }
}
