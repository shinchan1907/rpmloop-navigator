package io.bytenex.rpmloop.tiles

import android.content.Context
import android.content.SharedPreferences
import java.io.File

/**
 * On-phone storage for offline tiles plus the small bits of persistent
 * state the tiles feature needs:
 *  - tile JPEGs under filesDir/tiles/{z}/{x}/{y}.jpg (mirrors the SD layout);
 *  - the user's Google Maps Tile API key (app-private SharedPreferences,
 *    entered in Settings — never hardcoded, never logged);
 *  - the sent-manifest: device paths of tiles confirmed transferred
 *    (file_done received). Files are only ever marked sent on confirmation;
 *    anything else is (re)sent, and the device-side resume offset keeps
 *    re-sends cheap.
 */
class TileStore(context: Context) {

    private val prefs: SharedPreferences =
        context.getSharedPreferences("rpmloop_tiles", Context.MODE_PRIVATE)

    val tilesRoot: File = File(context.filesDir, "tiles")

    fun localFile(coord: TileCoord): File =
        File(tilesRoot, "${coord.z}/${coord.x}/${coord.y}.jpg")

    fun isDownloaded(coord: TileCoord): Boolean {
        val f = localFile(coord)
        return f.isFile && f.length() > 0L
    }

    /** Every tile currently on the phone, sorted for deterministic sending. */
    fun downloadedTiles(): List<TileCoord> {
        val out = ArrayList<TileCoord>()
        val zooms = tilesRoot.listFiles() ?: return emptyList()
        for (zDir in zooms) {
            val z = zDir.name.toIntOrNull() ?: continue
            if (!zDir.isDirectory) continue
            for (xDir in zDir.listFiles() ?: emptyArray()) {
                val x = xDir.name.toIntOrNull() ?: continue
                if (!xDir.isDirectory) continue
                for (f in xDir.listFiles() ?: emptyArray()) {
                    val y = f.nameWithoutExtension.toIntOrNull() ?: continue
                    if (f.extension == "jpg" && f.length() > 0L) {
                        out.add(TileCoord(z, x, y))
                    }
                }
            }
        }
        return out.sortedWith(compareBy({ it.z }, { it.x }, { it.y }))
    }

    fun downloadedCount(): Int = downloadedTiles().size

    // ---- API key ---------------------------------------------------------------

    fun apiKey(): String? = prefs.getString(KEY_API_KEY, null)?.takeIf { it.isNotBlank() }

    fun setApiKey(key: String?) {
        prefs.edit().apply {
            if (key.isNullOrBlank()) remove(KEY_API_KEY) else putString(KEY_API_KEY, key.trim())
        }.apply()
    }

    fun hasApiKey(): Boolean = apiKey() != null

    // ---- sent-manifest -----------------------------------------------------------

    fun isSent(coord: TileCoord): Boolean =
        prefs.getStringSet(KEY_SENT_PATHS, emptySet())!!.contains(coord.devicePath())

    fun markSent(coord: TileCoord) {
        val current = prefs.getStringSet(KEY_SENT_PATHS, emptySet())!!
        if (current.contains(coord.devicePath())) return
        val updated = HashSet(current).apply { add(coord.devicePath()) }
        prefs.edit().putStringSet(KEY_SENT_PATHS, updated).apply()
    }

    fun sentCount(): Int = prefs.getStringSet(KEY_SENT_PATHS, emptySet())!!.size

    /** Tiles on the phone that the device has not yet confirmed. */
    fun pendingTiles(): List<TileCoord> = downloadedTiles().filter { !isSent(it) }

    companion object {
        private const val KEY_API_KEY = "tile_api_key"
        private const val KEY_SENT_PATHS = "sent_tile_paths"
    }
}
