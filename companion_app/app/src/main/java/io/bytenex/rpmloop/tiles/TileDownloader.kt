package io.bytenex.rpmloop.tiles

import android.util.Log
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicInteger

/**
 * Downloads Google Maps roadmap tiles to the phone (TileStore), via the
 * Google Maps Tile API:
 *   POST https://tile.googleapis.com/v1/createSession?key=KEY
 *        {"mapType":"roadmap","language":"en-US","region":"IN"}
 *   GET  https://tile.googleapis.com/v1/2dtiles/{z}/{x}/{y}?session=S&key=KEY
 *
 * The API key comes from TileStore (user-entered in Settings). It is only
 * ever placed in request URLs — never logged, never written anywhere else.
 *
 * Tiles already on disk are skipped. A failed tile is attempted up to
 * 3 times (an HTTP 401/403 refreshes the session once inside those tries).
 * Work runs on a small worker pool; [cancel] stops the job cooperatively.
 */
class TileDownloader(private val store: TileStore) {

    interface Listener {
        fun onProgress(done: Int, total: Int, failed: Int)
        fun onFinished(cancelled: Boolean, failed: Int, error: String?)
    }

    companion object {
        private const val TAG = "TileDownloader"
        private const val MAX_ATTEMPTS = 3
        private const val POOL_SIZE = 4
    }

    @Volatile
    private var cancelled = false

    private var pool: ExecutorService? = null
    private var coordinator: Thread? = null

    @Volatile
    private var session: String? = null

    @Volatile
    private var sessionKey: String? = null

    val isRunning: Boolean get() = coordinator?.isAlive == true

    fun start(tiles: List<TileCoord>, apiKey: String, listener: Listener) {
        if (isRunning) {
            listener.onFinished(false, 0, "A download is already running")
            return
        }
        cancelled = false
        val todo = tiles.filter { !store.isDownloaded(it) }
        val total = tiles.size
        val skipped = total - todo.size
        if (todo.isEmpty()) {
            listener.onProgress(total, total, 0)
            listener.onFinished(false, 0, null)
            return
        }
        val done = AtomicInteger(skipped)
        val failed = AtomicInteger(0)
        listener.onProgress(done.get(), total, 0)
        val workers = Executors.newFixedThreadPool(POOL_SIZE)
        pool = workers
        coordinator = Thread {
            try {
                for (coord in todo) {
                    workers.execute {
                        if (cancelled) return@execute
                        val ok = downloadWithRetries(coord, apiKey)
                        if (!ok) failed.incrementAndGet()
                        val d = done.incrementAndGet()
                        listener.onProgress(d, total, failed.get())
                    }
                }
                workers.shutdown()
                while (!workers.awaitTermination(1, java.util.concurrent.TimeUnit.SECONDS)) {
                    if (cancelled) workers.shutdownNow()
                }
            } catch (e: InterruptedException) {
                workers.shutdownNow()
            } finally {
                pool = null
                listener.onFinished(cancelled, failed.get(), null)
            }
        }.apply { name = "rpmloop-tile-download"; start() }
    }

    fun cancel() {
        cancelled = true
    }

    // ---- one tile --------------------------------------------------------------

    private fun downloadWithRetries(coord: TileCoord, apiKey: String): Boolean {
        var attempt = 0
        while (attempt < MAX_ATTEMPTS) {
            attempt++
            try {
                val s = ensureSession(apiKey) ?: return false
                val url = URL(
                    "https://tile.googleapis.com/v1/2dtiles/${coord.z}/${coord.x}/${coord.y}" +
                        "?session=${URLEncoder.encode(s, "UTF-8")}" +
                        "&key=${URLEncoder.encode(apiKey, "UTF-8")}"
                )
                val conn = (url.openConnection() as HttpURLConnection).apply {
                    requestMethod = "GET"
                    connectTimeout = 10_000
                    readTimeout = 15_000
                }
                try {
                    when (val code = conn.responseCode) {
                        HttpURLConnection.HTTP_OK -> {
                            val bytes = conn.inputStream.use { it.readBytes() }
                            if (bytes.isEmpty()) continue
                            writeTile(coord, bytes)
                            return true
                        }
                        HttpURLConnection.HTTP_UNAUTHORIZED,
                        HttpURLConnection.HTTP_FORBIDDEN -> {
                            // Stale session (or bad key) — drop the session
                            // so the next attempt creates a fresh one; if the
                            // key itself is bad every attempt lands here and
                            // the tile honestly fails after 3 tries.
                            session = null
                            Log.w(TAG, "Tile ${coord.z}/${coord.x}/${coord.y}: HTTP $code")
                            continue
                        }
                        else -> {
                            Log.w(TAG, "Tile ${coord.z}/${coord.x}/${coord.y}: HTTP $code")
                            continue
                        }
                    }
                } finally {
                    conn.disconnect()
                }
            } catch (e: Exception) {
                Log.w(TAG, "Tile ${coord.z}/${coord.x}/${coord.y} attempt $attempt failed", e)
            }
            try {
                Thread.sleep(300L * attempt)
            } catch (e: InterruptedException) {
                return false
            }
        }
        return false
    }

    private fun writeTile(coord: TileCoord, bytes: ByteArray) {
        val target = store.localFile(coord)
        target.parentFile?.mkdirs()
        val tmp = File(target.parentFile, target.name + ".tmp")
        tmp.writeBytes(bytes)
        if (!tmp.renameTo(target)) {
            target.writeBytes(bytes)
            tmp.delete()
        }
    }

    // ---- session -----------------------------------------------------------------

    @Synchronized
    private fun ensureSession(apiKey: String): String? {
        val existing = session
        if (existing != null && sessionKey == apiKey) return existing
        val conn = (URL(
            "https://tile.googleapis.com/v1/createSession?key=" +
                URLEncoder.encode(apiKey, "UTF-8")
        ).openConnection() as HttpURLConnection).apply {
            requestMethod = "POST"
            doOutput = true
            connectTimeout = 10_000
            readTimeout = 15_000
            setRequestProperty("Content-Type", "application/json")
        }
        return try {
            val body = JSONObject().apply {
                put("mapType", "roadmap")
                put("language", "en-US")
                put("region", "IN")
            }.toString()
            conn.outputStream.use { it.write(body.toByteArray(Charsets.UTF_8)) }
            if (conn.responseCode != HttpURLConnection.HTTP_OK) {
                Log.w(TAG, "createSession failed: HTTP ${conn.responseCode}")
                null
            } else {
                val text = conn.inputStream.bufferedReader().use { it.readText() }
                JSONObject(text).optString("session").takeIf { it.isNotEmpty() }?.also {
                    session = it
                    sessionKey = apiKey
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "createSession failed", e)
            null
        } finally {
            conn.disconnect()
        }
    }
}
