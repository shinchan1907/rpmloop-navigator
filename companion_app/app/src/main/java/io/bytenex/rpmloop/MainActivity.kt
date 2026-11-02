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
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import io.bytenex.rpmloop.ble.BleConnectionState
import io.bytenex.rpmloop.databinding.ActivityMainBinding
import io.bytenex.rpmloop.service.AppUiState
import io.bytenex.rpmloop.service.CompanionService
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import java.util.Locale

/**
 * Single Phase 1 screen: connection status, Start/Stop, handshake result and
 * the live GPS readout. All real values come from CompanionService's state;
 * anything unavailable is shown as "—", never invented.
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    private var service: CompanionService? = null
    private var bound = false
    private var collectJob: Job? = null

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
    }
}
