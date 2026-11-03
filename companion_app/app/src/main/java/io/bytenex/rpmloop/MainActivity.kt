package io.bytenex.rpmloop

import android.Manifest
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import android.view.View
import android.view.inputmethod.EditorInfo
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import io.bytenex.rpmloop.ble.BleConnectionState
import io.bytenex.rpmloop.databinding.ActivityMainBinding
import io.bytenex.rpmloop.nav.GeocodingClient
import io.bytenex.rpmloop.nav.SearchResult
import io.bytenex.rpmloop.service.AppUiState
import io.bytenex.rpmloop.service.CompanionService
import io.bytenex.rpmloop.tiles.TileStore
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import java.util.Locale

/**
 * Main screen: connection status + Start/Stop, live GPS readout, the
 * Navigation section (search / waypoints / plan / live turn guidance) and
 * the Offline tiles section (API key, downloads, device transfer).
 *
 * All durable state lives in CompanionService (navigation and transfers run
 * in the foreground service, screen-off safe); this Activity only renders
 * its state flow and forwards user actions. Anything unavailable is shown
 * as "—" or an explicit message — never invented.
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    private var service: CompanionService? = null
    private var bound = false
    private var collectJob: Job? = null

    /** Read-only fallback for the tiles key/count readouts before the
     *  service has ever started (same SharedPreferences / files dir). */
    private val localTileStore by lazy { TileStore(applicationContext) }

    private var searchResults: List<SearchResult> = emptyList()
    private var searching = false

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            val svc = (binder as? CompanionService.LocalBinder)?.getService() ?: return
            service = svc
            bound = true
            observeService(svc)
            binding.btnToggle.text = if (CompanionService.isRunning) "Stop" else "Start"
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            service = null
            bound = false
            collectJob?.cancel()
            render(AppUiState())
        }
    }

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { _ ->
        val missing = missingPermissions()
        if (missing.isEmpty()) {
            hideNotice()
            startCompanion()
        } else {
            val names = missing.joinToString(", ") { friendlyPermissionName(it) }
            showNotice("Still needed: $names. The app cannot run without them.")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        render(AppUiState())
        binding.btnToggle.setOnClickListener { onToggleClicked() }
        wireNavigation()
        wireTiles()
    }

    override fun onStart() {
        super.onStart()
        // Re-attach to a service that is already running (e.g. after rotation).
        if (CompanionService.isRunning && !bound) {
            bindService(
                Intent(this, CompanionService::class.java),
                serviceConnection,
                0
            )
        }
        binding.btnToggle.text = if (CompanionService.isRunning) "Stop" else "Start"
    }

    override fun onStop() {
        if (bound) {
            try {
                unbindService(serviceConnection)
            } catch (e: IllegalArgumentException) {
                // Already unbound.
            }
            bound = false
            service = null
        }
        collectJob?.cancel()
        super.onStop()
    }

    // ---- start / stop ---------------------------------------------------------

    private fun onToggleClicked() {
        if (CompanionService.isRunning) {
            service?.stopEverything()
            if (bound) {
                try {
                    unbindService(serviceConnection)
                } catch (e: IllegalArgumentException) {
                    // Already unbound.
                }
                bound = false
                service = null
            }
            collectJob?.cancel()
            render(AppUiState())
            binding.btnToggle.text = "Start"
        } else {
            val missing = missingPermissions()
            if (missing.isEmpty()) {
                hideNotice()
                startCompanion()
            } else {
                permissionLauncher.launch(missing.toTypedArray())
            }
        }
    }

    private fun startCompanion() {
        val intent = Intent(this, CompanionService::class.java)
            .setAction(CompanionService.ACTION_START)
        ContextCompat.startForegroundService(this, intent)
        if (!bound) {
            bound = bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE)
        }
        binding.btnToggle.text = "Stop"
    }

    private fun observeService(svc: CompanionService) {
        collectJob?.cancel()
        collectJob = lifecycleScope.launch {
            svc.state.collect { render(it) }
        }
    }

    /** Navigation and tiles live in the foreground service; without it there
     *  is nothing to drive. Returns the service or explains why not. */
    private fun requireService(): CompanionService? {
        val svc = service
        if (svc == null) {
            showNotice("Press Start at the top first — navigation and tile downloads run in the background service.")
        }
        return svc
    }

    // ---- navigation wiring ------------------------------------------------------

    private fun wireNavigation() {
        binding.btnSearch.setOnClickListener { runSearch() }
        binding.etSearch.setOnEditorActionListener { _, actionId, _ ->
            if (actionId == EditorInfo.IME_ACTION_SEARCH) {
                runSearch()
                true
            } else {
                false
            }
        }
        binding.btnPlanRoute.setOnClickListener { requireService()?.planRoute() }
        binding.btnStartNav.setOnClickListener { requireService()?.startNavigation() }
        binding.btnStopNav.setOnClickListener { requireService()?.stopNavigation() }
    }

    private fun runSearch() {
        val query = binding.etSearch.text?.toString()?.trim().orEmpty()
        if (query.isEmpty() || searching) return
        searching = true
        binding.tvSearchStatus.visibility = View.VISIBLE
        binding.tvSearchStatus.text = "Searching…"
        binding.llSearchResults.removeAllViews()
        lifecycleScope.launch {
            try {
                val results = GeocodingClient.search(query)
                searchResults = results
                if (results.isEmpty()) {
                    binding.tvSearchStatus.text = "No places found for “$query”."
                } else {
                    binding.tvSearchStatus.visibility = View.GONE
                    renderSearchResults(results)
                }
            } catch (e: Exception) {
                binding.tvSearchStatus.text = "Search failed: ${e.message ?: "network error"}"
            } finally {
                searching = false
            }
        }
    }

    private fun renderSearchResults(results: List<SearchResult>) {
        val container = binding.llSearchResults
        container.removeAllViews()
        for (result in results) {
            val row = TextView(this).apply {
                text = result.name
                setPadding(8, 14, 8, 14)
                setOnClickListener { onSearchResultTapped(result) }
            }
            container.addView(
                row,
                LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                )
            )
        }
    }

    private fun onSearchResultTapped(result: SearchResult) {
        val svc = requireService() ?: return
        AlertDialog.Builder(this)
            .setTitle(shortName(result.name))
            .setItems(arrayOf("Add as stop (break point)", "Set as destination")) { _, which ->
                when (which) {
                    0 -> svc.addStop(result.name, result.point)
                    1 -> svc.setDestination(result.name, result.point)
                }
                searchResults = emptyList()
                binding.llSearchResults.removeAllViews()
                binding.etSearch.text?.clear()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun shortName(displayName: String): String =
        displayName.split(",").firstOrNull()?.trim()?.takeIf { it.isNotEmpty() } ?: displayName

    private fun renderWaypoints(state: AppUiState) {
        val rows = state.nav.waypoints
        binding.tvNoWaypoints.visibility = if (rows.isEmpty()) View.VISIBLE else View.GONE
        val container = binding.llWaypoints
        container.removeAllViews()
        rows.forEachIndexed { index, row ->
            val line = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                setPadding(0, 6, 0, 6)
            }
            val label = TextView(this).apply {
                text = if (row.isDestination) {
                    "Destination: ${shortName(row.name)}"
                } else {
                    "Stop ${index + 1}: ${shortName(row.name)}"
                }
                layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
            }
            val remove = Button(this).apply {
                text = "Remove"
                setOnClickListener { service?.removeWaypoint(index) }
            }
            line.addView(label)
            line.addView(remove)
            container.addView(line)
        }
    }

    // ---- tiles wiring -------------------------------------------------------------

    private fun wireTiles() {
        binding.btnSaveApiKey.setOnClickListener {
            val key = binding.etApiKey.text?.toString()?.trim().orEmpty()
            val svc = service
            if (svc != null) {
                svc.setTileApiKey(key.ifEmpty { null })
            } else {
                localTileStore.setApiKey(key.ifEmpty { null })
                render(AppUiState())
            }
            binding.etApiKey.text?.clear()
        }
        binding.btnRouteTiles.setOnClickListener {
            val svc = requireService() ?: return@setOnClickListener
            val estimate = svc.estimateRouteTiles()
            if (estimate == null) {
                showNotice("Plan a route first — route tiles cover the planned route.")
            } else {
                confirmDownload("Route tiles", estimate) { svc.startRouteTileDownload() }
            }
        }
        binding.btnNearbyTiles.setOnClickListener {
            val svc = requireService() ?: return@setOnClickListener
            val estimate = svc.estimateNearbyTiles()
            if (estimate == null) {
                showNotice("No GPS fix yet — nearby tiles are centred on your position.")
            } else {
                confirmDownload("Nearby tiles (5 km)", estimate) { svc.startNearbyTileDownload() }
            }
        }
        binding.btnCancelDownload.setOnClickListener { service?.cancelTileDownload() }
        binding.btnSendTiles.setOnClickListener { requireService()?.sendTilesToDevice() }
        binding.btnCancelTransfer.setOnClickListener { service?.cancelTileTransfer() }
    }

    private fun confirmDownload(
        jobName: String,
        estimate: Pair<Int, Long>,
        onConfirm: () -> Unit
    ) {
        val (count, bytes) = estimate
        val mb = bytes / 1_000_000.0
        AlertDialog.Builder(this)
            .setTitle(jobName)
            .setMessage(
                String.format(
                    Locale.US,
                    "Download %d tiles (zooms 13–16), about %.1f MB from Google Maps? " +
                        "Tiles already on the phone are skipped.",
                    count, mb
                )
            )
            .setPositiveButton("Download") { _, _ -> onConfirm() }
            .setNegativeButton("Cancel", null)
            .show()
    }

    // ---- permissions ------------------------------------------------------------

    private fun requiredPermissions(): List<String> {
        val list = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            list += Manifest.permission.BLUETOOTH_SCAN
            list += Manifest.permission.BLUETOOTH_CONNECT
        }
        list += Manifest.permission.ACCESS_FINE_LOCATION
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            list += Manifest.permission.POST_NOTIFICATIONS
        }
        return list
    }

    private fun missingPermissions(): List<String> = requiredPermissions().filter {
        ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
    }

    private fun friendlyPermissionName(permission: String): String = when (permission) {
        Manifest.permission.BLUETOOTH_SCAN -> "Nearby devices (scan)"
        Manifest.permission.BLUETOOTH_CONNECT -> "Nearby devices (connect)"
        Manifest.permission.ACCESS_FINE_LOCATION -> "Location"
        Manifest.permission.POST_NOTIFICATIONS -> "Notifications"
        else -> permission
    }

    // ---- rendering ----------------------------------------------------------------

    private fun showNotice(text: String) {
        binding.tvNotice.text = text
        binding.tvNotice.visibility = View.VISIBLE
    }

    private fun hideNotice() {
        binding.tvNotice.visibility = View.GONE
    }

    private fun render(state: AppUiState) {
        binding.tvStatus.text = when (state.connection) {
            BleConnectionState.DISCONNECTED -> "Disconnected"
            BleConnectionState.SCANNING -> "Scanning"
            BleConnectionState.CONNECTING -> "Connecting"
            BleConnectionState.CONNECTED -> "Connected"
        }
        binding.tvDevice.text = "Device: ${state.deviceName ?: "—"}"
        binding.tvHandshake.text = "Handshake: ${state.handshake ?: "—"}"
        if (state.detail != null) {
            showNotice(state.detail)
        } else if (CompanionService.isRunning) {
            hideNotice()
        }
        binding.tvLat.text = state.latitude?.let {
            "Latitude: ${String.format(Locale.US, "%.6f", it)}"
        } ?: "Latitude: —"
        binding.tvLon.text = state.longitude?.let {
            "Longitude: ${String.format(Locale.US, "%.6f", it)}"
        } ?: "Longitude: —"
        binding.tvSpeed.text = state.speedKmh?.let {
            "Speed: ${String.format(Locale.US, "%.1f", it)} km/h"
        } ?: "Speed: —"
        binding.tvHeading.text = state.headingDeg?.let {
            "Heading: ${String.format(Locale.US, "%.0f", it)}°"
        } ?: "Heading: —"
        binding.tvFixNote.text = when {
            state.latitude != null -> "Live — position updates about once a second."
            CompanionService.isRunning -> "Waiting for GPS fix…"
            else -> "Position appears here once GPS has a fix."
        }
        renderNav(state)
        renderTiles(state)
    }

    private fun renderNav(state: AppUiState) {
        renderWaypoints(state)
        val nav = state.nav
        binding.tvRouteSummary.visibility = if (nav.routeSummary != null) View.VISIBLE else View.GONE
        binding.tvRouteSummary.text = nav.routeSummary?.let { "Route: $it" }
        binding.btnPlanRoute.text = if (nav.planning) "Planning…" else "Plan route"
        binding.btnPlanRoute.isEnabled = !nav.planning
        binding.btnStartNav.isEnabled = !nav.navigating
        binding.btnStopNav.isEnabled = nav.navigating
        binding.tvNavNext.visibility = if (nav.nextTurn != null) View.VISIBLE else View.GONE
        binding.tvNavNext.text = nav.nextTurn
        binding.tvNavRemaining.visibility = if (nav.remaining != null) View.VISIBLE else View.GONE
        binding.tvNavRemaining.text = nav.remaining
        binding.tvNavMessage.visibility = if (nav.message != null) View.VISIBLE else View.GONE
        binding.tvNavMessage.text = nav.message
    }

    private fun renderTiles(state: AppUiState) {
        val tiles = state.tiles
        // Before the service has ever run, fall back to the local store so a
        // previously saved key / existing tiles don't read as "none".
        val hasKey = tiles.hasApiKey || (service == null && localTileStore.hasApiKey())
        val count = if (service == null) localTileStore.downloadedCount() else tiles.downloadedCount
        binding.tvApiKeyState.text = if (hasKey) {
            "Key saved — downloads enabled. Type a new key and Save to replace it."
        } else {
            "No key saved — downloads are disabled."
        }
        binding.tvDownloadedCount.text = "Tiles on phone: $count"
        binding.pbDownload.visibility = if (tiles.downloading) View.VISIBLE else View.GONE
        binding.tvDownloadProgress.visibility = if (tiles.downloading) View.VISIBLE else View.GONE
        binding.btnCancelDownload.visibility = if (tiles.downloading) View.VISIBLE else View.GONE
        if (tiles.downloading) {
            binding.pbDownload.max = tiles.downloadTotal.coerceAtLeast(1)
            binding.pbDownload.progress = tiles.downloadDone
            binding.tvDownloadProgress.text = buildString {
                append(tiles.downloadJob ?: "Tiles")
                append(" · ").append(tiles.downloadDone).append(" / ").append(tiles.downloadTotal)
                if (tiles.downloadFailed > 0) append(" · ").append(tiles.downloadFailed).append(" failed")
            }
        }
        binding.pbTransfer.visibility = if (tiles.transferring) View.VISIBLE else View.GONE
        binding.tvTransferProgress.visibility = if (tiles.transferring) View.VISIBLE else View.GONE
        binding.btnCancelTransfer.visibility = if (tiles.transferring) View.VISIBLE else View.GONE
        binding.btnSendTiles.isEnabled = !tiles.transferring
        if (tiles.transferring) {
            val total = tiles.transferFilesTotal.coerceAtLeast(1)
            val overall = ((tiles.transferFilesDone + tiles.transferFilePct / 100.0) / total * 100).toInt()
            binding.pbTransfer.max = 100
            binding.pbTransfer.progress = overall.coerceIn(0, 100)
            binding.tvTransferProgress.text = buildString {
                append("Sending · file ").append(tiles.transferFilesDone + 1)
                append(" of ").append(tiles.transferFilesTotal)
                tiles.transferCurrentFile?.let { append(" · ").append(it) }
                append(" · ").append(tiles.transferFilePct).append("%")
                if (tiles.transferKBps > 0) append(" · ~").append(tiles.transferKBps).append(" KB/s")
            }
        }
        binding.tvTilesMessage.visibility = if (tiles.message != null) View.VISIBLE else View.GONE
        binding.tvTilesMessage.text = tiles.message
    }
}
