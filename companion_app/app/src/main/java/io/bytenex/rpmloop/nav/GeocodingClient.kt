package io.bytenex.rpmloop.nav

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder

/**
 * Destination search against OpenStreetMap's Nominatim service.
 * Nominatim's usage policy requires a valid, app-identifying User-Agent,
 * which is set on every request. Results are real only — any failure throws
 * and the UI shows the error; nothing is fabricated.
 */
object GeocodingClient {

    private const val USER_AGENT = "RPMLOOP Companion/2.0 (io.bytenex.rpmloop; Android)"

    suspend fun search(query: String): List<SearchResult> = withContext(Dispatchers.IO) {
        val q = URLEncoder.encode(query.trim(), "UTF-8")
        val url = URL("https://nominatim.openstreetmap.org/search?format=json&limit=5&q=$q")
        val conn = (url.openConnection() as HttpURLConnection).apply {
            requestMethod = "GET"
            connectTimeout = 10_000
            readTimeout = 15_000
            setRequestProperty("User-Agent", USER_AGENT)
            setRequestProperty("Accept", "application/json")
        }
        try {
            val code = conn.responseCode
            if (code != HttpURLConnection.HTTP_OK) {
                throw IllegalStateException("Place search failed (HTTP $code)")
            }
            val body = conn.inputStream.bufferedReader().use { it.readText() }
            parseResults(body)
        } finally {
            conn.disconnect()
        }
    }

    private fun parseResults(body: String): List<SearchResult> {
        val arr = JSONArray(body)
        val out = ArrayList<SearchResult>(arr.length())
        for (i in 0 until arr.length()) {
            val o = arr.optJSONObject(i) ?: continue
            val lat = o.optDouble("lat", Double.NaN)
            val lon = o.optDouble("lon", Double.NaN)
            val name = o.optString("display_name")
            if (lat.isNaN() || lon.isNaN() || name.isEmpty()) continue
            out.add(SearchResult(name, GeoPoint(lat, lon)))
        }
        return out
    }
}
