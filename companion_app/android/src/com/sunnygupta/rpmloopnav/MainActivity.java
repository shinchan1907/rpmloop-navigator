package com.sunnygupta.rpmloopnav;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.text.TextUtils;
import android.webkit.GeolocationPermissions;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class MainActivity extends Activity implements BleManager.BleListener, GoogleMapsNotificationService.GoogleMapsListener, LocationListener {

    private static final int PERMISSION_REQ_CODE = 101;
    private WebView webView;
    private BleManager bleManager;
    private LocationManager locationManager;
    private Location lastKnownLocation;

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        webView = findViewById(R.id.webview);
        bleManager = new BleManager(this, this);
        locationManager = (LocationManager) getSystemService(Context.LOCATION_SERVICE);

        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setGeolocationEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowContentAccess(true);
        settings.setCacheMode(WebSettings.LOAD_DEFAULT);

        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onGeolocationPermissionsShowPrompt(String origin, GeolocationPermissions.Callback callback) {
                // Grant geolocation access without prompt for WebView asset origin
                callback.invoke(origin, true, false);
            }
        });
        webView.setWebViewClient(new WebViewClient());
        webView.addJavascriptInterface(new WebAppInterface(), "AndroidBleBridge");

        GoogleMapsNotificationService.setListener(this);

        checkPermissions();
        webView.loadUrl("file:///android_asset/index.html");
    }

    private void checkPermissions() {
        List<String> needed = new ArrayList<>();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_SCAN);
            }
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_CONNECT);
            }
        }
        if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            needed.add(Manifest.permission.ACCESS_FINE_LOCATION);
        }
        if (checkSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            needed.add(Manifest.permission.ACCESS_COARSE_LOCATION);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.POST_NOTIFICATIONS);
            }
        }

        if (!needed.isEmpty()) {
            requestPermissions(needed.toArray(new String[0]), PERMISSION_REQ_CODE);
        } else {
            startLocationUpdates();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSION_REQ_CODE) {
            boolean fineGranted = checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
            if (fineGranted) {
                onLog("INFO", "Location & Bluetooth permissions active.");
                startLocationUpdates();
            } else {
                onLog("WARN", "High-accuracy GPS location permission not granted.");
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED) {
            startLocationUpdates();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        // Keep listening or pause if necessary. We keep listening for uninterrupted navigation.
    }

    private void startLocationUpdates() {
        if (locationManager == null) return;
        try {
            // First check cached last known location
            Location gpsLoc = locationManager.getLastKnownLocation(LocationManager.GPS_PROVIDER);
            Location netLoc = locationManager.getLastKnownLocation(LocationManager.NETWORK_PROVIDER);
            Location best = (gpsLoc != null) ? gpsLoc : netLoc;
            if (best != null) {
                lastKnownLocation = best;
                sendLocationToJs(best);
            }

            if (locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 1000, 1.0f, this);
            }
            if (locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
                locationManager.requestLocationUpdates(LocationManager.NETWORK_PROVIDER, 2000, 2.0f, this);
            }
        } catch (SecurityException e) {
            onLog("WARN", "Location tracking error: " + e.getMessage());
        }
    }

    @Override
    public void onLocationChanged(Location location) {
        if (location == null) return;
        lastKnownLocation = location;
        sendLocationToJs(location);
    }

    @Override
    public void onStatusChanged(String provider, int status, Bundle extras) {}

    @Override
    public void onProviderEnabled(String provider) {
        onLog("INFO", "Location provider enabled: " + provider);
    }

    @Override
    public void onProviderDisabled(String provider) {
        onLog("WARN", "Location provider disabled: " + provider);
    }

    private void sendLocationToJs(Location loc) {
        double lat = loc.getLatitude();
        double lng = loc.getLongitude();
        float accuracy = loc.hasAccuracy() ? loc.getAccuracy() : 10.0f;
        float speedKmh = loc.hasSpeed() ? (loc.getSpeed() * 3.6f) : 0f;
        float bearing = loc.hasBearing() ? loc.getBearing() : 0f;
        double altitude = loc.hasAltitude() ? loc.getAltitude() : 0.0;

        runOnUiThread(() -> {
            String js = String.format(Locale.US,
                    "window.onLocationUpdate && window.onLocationUpdate(%.6f, %.6f, %.1f, %.1f, %.1f, %.1f);",
                    lat, lng, accuracy, speedKmh, bearing, altitude);
            webView.evaluateJavascript(js, null);
        });
    }

    // Google Maps Mirroring Callbacks
    @Override
    public void onGoogleMapsNavUpdate(String title, String text, String maneuver, int distMeters, String road) {
        runOnUiThread(() -> {
            String escapedTitle = escapeForJs(title);
            String escapedText = escapeForJs(text);
            String escapedRoad = escapeForJs(road);
            String js = String.format("window.onGoogleMapsNav && window.onGoogleMapsNav('%s', '%s', '%s', %d, '%s');",
                    escapedTitle, escapedText, maneuver, distMeters, escapedRoad);
            webView.evaluateJavascript(js, null);
        });
    }

    // BLE Callbacks -> JavaScript
    @Override
    public void onConnectionStateChange(String status, String deviceName, int rssi) {
        runOnUiThread(() -> {
            String js = String.format("window.onBleStatus && window.onBleStatus('%s', '%s', %d);", status, deviceName, rssi);
            webView.evaluateJavascript(js, null);
        });
    }

    @Override
    public void onAckReceived(String payload) {
        runOnUiThread(() -> {
            String escaped = escapeForJs(payload);
            String js = String.format("window.onBleAck && window.onBleAck('%s');", escaped);
            webView.evaluateJavascript(js, null);
        });
    }

    @Override
    public void onLog(String level, String message) {
        runOnUiThread(() -> {
            String escaped = escapeForJs(message);
            String js = String.format("window.onBleLog && window.onBleLog('%s', '%s');", level, escaped);
            webView.evaluateJavascript(js, null);
        });
    }

    @Override
    public void onRssiUpdated(int rssi) {
        runOnUiThread(() -> {
            String js = String.format("window.onBleRssi && window.onBleRssi(%d);", rssi);
            webView.evaluateJavascript(js, null);
        });
    }

    private String escapeForJs(String str) {
        if (str == null) return "";
        return str.replace("\\", "\\\\").replace("'", "\\'").replace("\n", " ").replace("\r", " ");
    }

    // JavaScript Bridge
    public class WebAppInterface {
        @JavascriptInterface
        public void connect() {
            bleManager.startScan();
        }

        @JavascriptInterface
        public void stopScan() {
            bleManager.stopScan();
        }

        @JavascriptInterface
        public boolean isBleConnected() {
            return bleManager != null && bleManager.isConnected();
        }

        @JavascriptInterface
        public void disconnect() {
            bleManager.disconnect();
        }

        @JavascriptInterface
        public boolean sendCommand(String json) {
            return bleManager.writeJsonCommand(json);
        }

        @JavascriptInterface
        public void setBrightness(int percent) {
            String payload = String.format("{\"v\":1,\"type\":\"SET_BRIGHTNESS\",\"seq\":%d,\"sid\":\"android\",\"payload\":{\"brightness\":%d}}",
                    System.currentTimeMillis() % 100000, percent);
            bleManager.writeJsonCommand(payload);
        }

        @JavascriptInterface
        public void ping() {
            String payload = String.format("{\"v\":1,\"type\":\"PING\",\"seq\":%d,\"sid\":\"android\"}",
                    System.currentTimeMillis() % 100000);
            bleManager.writeJsonCommand(payload);
        }

        @JavascriptInterface
        public String getInitialLocationJson() {
            if (lastKnownLocation != null) {
                return String.format(Locale.US,
                        "{\"lat\":%.6f,\"lng\":%.6f,\"accuracy\":%.1f,\"speed\":%.1f,\"bearing\":%.1f}",
                        lastKnownLocation.getLatitude(), lastKnownLocation.getLongitude(),
                        lastKnownLocation.hasAccuracy() ? lastKnownLocation.getAccuracy() : 10.0f,
                        lastKnownLocation.hasSpeed() ? lastKnownLocation.getSpeed() * 3.6f : 0f,
                        lastKnownLocation.hasBearing() ? lastKnownLocation.getBearing() : 0f);
            }
            return "{}";
        }

        @JavascriptInterface
        public boolean isGpsEnabled() {
            if (locationManager == null) return false;
            return locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER);
        }

        @JavascriptInterface
        public void openLocationSettings() {
            runOnUiThread(() -> {
                try {
                    Intent intent = new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS);
                    startActivity(intent);
                } catch (Exception e) {
                    Toast.makeText(MainActivity.this, "Cannot open GPS settings", Toast.LENGTH_SHORT).show();
                }
            });
        }

        @JavascriptInterface
        public void copyToClipboard(String text) {
            runOnUiThread(() -> {
                ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
                ClipData clip = ClipData.newPlainText("RPMLoop Logs", text);
                clipboard.setPrimaryClip(clip);
                Toast.makeText(MainActivity.this, "Logs copied to clipboard!", Toast.LENGTH_SHORT).show();
            });
        }

        @JavascriptInterface
        public boolean isNotificationAccessGranted() {
            String pkgName = getPackageName();
            final String flat = Settings.Secure.getString(getContentResolver(), "enabled_notification_listeners");
            if (!TextUtils.isEmpty(flat)) {
                final String[] names = flat.split(":");
                for (String name : names) {
                    final ComponentName cn = ComponentName.unflattenFromString(name);
                    if (cn != null && TextUtils.equals(pkgName, cn.getPackageName())) {
                        return true;
                    }
                }
            }
            return false;
        }

        @JavascriptInterface
        public void openNotificationSettings() {
            runOnUiThread(() -> {
                try {
                    Intent intent = new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS);
                    startActivity(intent);
                    Toast.makeText(MainActivity.this, "Enable 'RPMLoop Nav' in the list to mirror Google Maps!", Toast.LENGTH_LONG).show();
                } catch (Exception e) {
                    Toast.makeText(MainActivity.this, "Could not open settings: " + e.getMessage(), Toast.LENGTH_SHORT).show();
                }
            });
        }

        @JavascriptInterface
        public void launchGoogleMaps(String destination) {
            runOnUiThread(() -> {
                try {
                    Uri gmmIntentUri = Uri.parse("google.navigation:q=" + Uri.encode(destination));
                    Intent mapIntent = new Intent(Intent.ACTION_VIEW, gmmIntentUri);
                    mapIntent.setPackage("com.google.android.apps.maps");
                    if (mapIntent.resolveActivity(getPackageManager()) != null) {
                        startActivity(mapIntent);
                    } else {
                        Intent webIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://www.google.com/maps/dir/?api=1&destination=" + Uri.encode(destination)));
                        startActivity(webIntent);
                    }
                } catch (Exception e) {
                    Toast.makeText(MainActivity.this, "Error launching Google Maps: " + e.getMessage(), Toast.LENGTH_SHORT).show();
                }
            });
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        GoogleMapsNotificationService.setListener(null);
        if (locationManager != null) {
            try {
                locationManager.removeUpdates(this);
            } catch (SecurityException ignored) {}
        }
        if (bleManager != null) {
            bleManager.disconnect();
        }
    }
}
