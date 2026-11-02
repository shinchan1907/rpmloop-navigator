package io.bytenex.rpmloop.location

import android.annotation.SuppressLint
import android.content.Context
import android.location.Location
import android.os.Looper
import com.google.android.gms.location.FusedLocationProviderClient
import com.google.android.gms.location.LocationCallback
import com.google.android.gms.location.LocationRequest
import com.google.android.gms.location.LocationResult
import com.google.android.gms.location.LocationServices
import com.google.android.gms.location.Priority

/**
 * Thin wrapper over FusedLocationProviderClient delivering fixes at ~1 Hz.
 * Assumes ACCESS_FINE_LOCATION has already been granted by the caller.
 */
class LocationProvider(
    context: Context,
    private val onLocation: (Location) -> Unit
) {

    private val client: FusedLocationProviderClient =
        LocationServices.getFusedLocationProviderClient(context)

    private val callback = object : LocationCallback() {
        override fun onLocationResult(result: LocationResult) {
            result.lastLocation?.let(onLocation)
        }
    }

    private var running = false

    @SuppressLint("MissingPermission")
    fun start() {
        if (running) return
        val request = LocationRequest.Builder(Priority.PRIORITY_HIGH_ACCURACY, 1000L)
            .setMinUpdateIntervalMillis(1000L)
            .build()
        try {
            client.requestLocationUpdates(request, callback, Looper.getMainLooper())
            running = true
        } catch (e: SecurityException) {
            // Location permission was revoked after the check; stay stopped and
            // let the UI keep its honest "no fix" state.
        }
    }

    fun stop() {
        if (!running) return
        running = false
        client.removeLocationUpdates(callback)
    }
}
