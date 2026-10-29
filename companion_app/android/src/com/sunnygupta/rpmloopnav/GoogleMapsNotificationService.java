package com.sunnygupta.rpmloopnav;

import android.app.Notification;
import android.os.Bundle;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.util.Log;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class GoogleMapsNotificationService extends NotificationListenerService {

    private static final String TAG = "RPMLoopGmaps";
    private static final String GMAPS_PKG = "com.google.android.apps.maps";

    public interface GoogleMapsListener {
        void onGoogleMapsNavUpdate(String title, String text, String maneuver, int distMeters, String road);
    }

    private static GoogleMapsListener activeListener;
    private static boolean isServiceActive = false;
    private static long lastUpdateTs = 0;
    private static int seqCounter = 100;

    public static void setListener(GoogleMapsListener listener) {
        activeListener = listener;
    }

    public static boolean isRunning() {
        return isServiceActive;
    }

    @Override
    public void onListenerConnected() {
        super.onListenerConnected();
        isServiceActive = true;
        Log.i(TAG, "Google Maps Notification Listener connected.");
    }

    @Override
    public void onListenerDisconnected() {
        super.onListenerDisconnected();
        isServiceActive = false;
        Log.w(TAG, "Google Maps Notification Listener disconnected.");
    }

    @Override
    public void onNotificationPosted(StatusBarNotification sbn) {
        if (sbn == null || !GMAPS_PKG.equals(sbn.getPackageName())) {
            return;
        }

        Notification notification = sbn.getNotification();
        if (notification == null) return;

        Bundle extras = notification.extras;
        if (extras == null) return;

        CharSequence titleCs = extras.getCharSequence(Notification.EXTRA_TITLE);
        CharSequence textCs = extras.getCharSequence(Notification.EXTRA_TEXT);
        CharSequence subTextCs = extras.getCharSequence(Notification.EXTRA_SUB_TEXT);

        String title = titleCs != null ? titleCs.toString() : "";
        String text = textCs != null ? textCs.toString() : "";
        String subText = subTextCs != null ? subTextCs.toString() : "";

        if (title.isEmpty() && text.isEmpty()) return;

        // Rate limit updates to 1 per 750ms to prevent BLE traffic flooding
        long now = System.currentTimeMillis();
        if (now - lastUpdateTs < 750) {
            return;
        }
        lastUpdateTs = now;

        parseAndTransmitGoogleMapsNav(title, text, subText);
    }

    private void parseAndTransmitGoogleMapsNav(String title, String text, String subText) {
        String combined = (title + " " + text + " " + subText).toLowerCase();

        // 1. Determine Maneuver Type
        String maneuver = "STRAIGHT";
        if (combined.contains("u-turn") || combined.contains("uturn")) {
            maneuver = "UTURN";
        } else if (combined.contains("sharp right")) {
            maneuver = "SHARP_RIGHT";
        } else if (combined.contains("slight right") || combined.contains("keep right") || combined.contains("bear right")) {
            maneuver = "SLIGHT_RIGHT";
        } else if (combined.contains("right")) {
            maneuver = "TURN_RIGHT";
        } else if (combined.contains("sharp left")) {
            maneuver = "SHARP_LEFT";
        } else if (combined.contains("slight left") || combined.contains("keep left") || combined.contains("bear left")) {
            maneuver = "SLIGHT_LEFT";
        } else if (combined.contains("left")) {
            maneuver = "TURN_LEFT";
        } else if (combined.contains("roundabout") || combined.contains("rotary") || combined.contains("traffic circle")) {
            maneuver = "ROUNDABOUT";
        } else if (combined.contains("destination") || combined.contains("arrived") || combined.contains("you have arrived")) {
            maneuver = "DESTINATION";
        }

        // 2. Extract Distance (meters)
        int distMeters = 200; // default reasonable estimate
        Pattern pDist = Pattern.compile("([0-9.]+)\\s*(m|meter|meters|km|kilometer|kilometers|ft|mi|miles)");
        Matcher mDist = pDist.matcher(combined);
        if (mDist.find()) {
            try {
                double val = Double.parseDouble(mDist.group(1));
                String unit = mDist.group(2).toLowerCase();
                // IMPORTANT: longer/more-specific prefixes checked FIRST.
                // "mi"/"miles" must come before "m"/"meters" — otherwise
                // startsWith("m") matches "miles" and treats the value as
                // metres instead of converting from imperial miles.
                if (unit.startsWith("km")) {
                    distMeters = (int) (val * 1000);
                } else if (unit.startsWith("mi")) {
                    distMeters = (int) (val * 1609.34);
                } else if (unit.startsWith("ft")) {
                    distMeters = (int) (val * 0.3048);
                } else if (unit.startsWith("m")) {
                    // "m", "meter", "meters" — intentionally last
                    distMeters = (int) val;
                }
            } catch (Exception ignored) {}
        }

        // 3. Extract Road Name
        String road = title;
        road = road.replaceAll("(?i)^(in\\s+[0-9.]+\\s*(m|km|ft|mi)[,\\s]*)", "");
        road = road.replaceAll("(?i)^(turn\\s+(right|left|slight right|slight left|sharp right|sharp left)\\s+(on|onto|to)\\s*)", "");
        road = road.replaceAll("(?i)^(continue\\s+(on|onto)\\s*)", "");
        road = road.replaceAll("(?i)^(head\\s+(north|south|east|west)\\s*(on|onto)?\\s*)", "");
        if (road.trim().isEmpty()) {
            road = !subText.isEmpty() ? subText : "Google Maps Route";
        }

        // 4. Send Packet via BLE to ESP32
        BleManager ble = BleManager.getInstance();
        if (ble != null) {
            seqCounter++;
            String jsonPacket = String.format(
                    "{\"v\":1,\"type\":\"NAV_UPDATE\",\"seq\":%d,\"sid\":\"gmaps\",\"payload\":{" +
                            "\"maneuver\":\"%s\"," +
                            "\"dist_m\":%d," +
                            "\"road\":\"%s\"," +
                            "\"instr\":\"%s\"," +
                            "\"speed_kmh\":0," +
                            "\"rem_dist_m\":0," +
                            "\"rem_time_s\":0" +
                            "}}",
                    seqCounter,
                    maneuver,
                    distMeters,
                    escapeJson(road),
                    escapeJson(title.isEmpty() ? text : title)
            );
            ble.writeJsonCommand(jsonPacket);
        }

        // 5. Notify UI Listener
        if (activeListener != null) {
            final String fManeuver = maneuver;
            final int fDist = distMeters;
            final String fRoad = road;
            activeListener.onGoogleMapsNavUpdate(title, text, fManeuver, fDist, fRoad);
        }
    }

    private static String escapeJson(String s) {
        if (s == null) return "";
        return s.replace("\\", "\\\\")
                .replace("\"", "\\\"")
                .replace("\n", " ")
                .replace("\r", " ");
    }
}
