package io.bytenex.rpmloop.ble

import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import org.json.JSONObject
import java.io.File

/**
 * Protocol v3.1 file transfer: pushes one file (an offline map tile) from
 * the phone to the device's SD card over the FILE_CTRL / FILE_DATA
 * characteristics of the RPMLOOP service.
 *
 * Wire protocol (implemented exactly):
 *  1. App writes {"type":"file_begin","path":...,"size":N} on FILE_CTRL.
 *  2. Device replies (FILE_CTRL notification) {"type":"file_ready",
 *     "offset":N} — the byte offset it already has, so transfers resume.
 *  3. App streams the remaining bytes on FILE_DATA as chunks:
 *     [seq u16 LE][payload_len u16 LE][payload], payload at most
 *     (negotiated MTU - 3 - 4) bytes. seq counts chunks from 0, starting at
 *     the chunk that carries the resume offset.
 *  4. The device acks with {"type":"file_ack","seq":K} = last contiguous
 *     chunk received. The app never has more than [WINDOW] chunks unacked.
 *  5. When every chunk is acked, the app writes {"type":"file_end",
 *     "size":N}; the device answers {"type":"file_done"} or
 *     {"type":"file_error","reason":...}.
 *
 * All state lives on a dedicated HandlerThread. One file at a time; the
 * caller (TileTransferManager) sequences files and handles retry policy.
 * A link drop aborts the in-flight file with an error — the device-side
 * partial file plus the file_ready offset make the next run resume it.
 */
class FileTransferClient(private val ble: BleManager) {

    interface Callback {
        /** [percent] = bytes the device has acknowledged, 0..100. */
        fun onProgress(percent: Int)
        fun onComplete()
        fun onError(reason: String)
    }

    companion object {
        private const val TAG = "FileTransfer"
        private const val WINDOW = 16
        private const val READY_TIMEOUT_MS = 10_000L
        private const val DONE_TIMEOUT_MS = 15_000L
        private const val STALL_TIMEOUT_MS = 15_000L
        private const val WRITE_RETRY_DELAY_MS = 25L
        private const val WRITE_PACE_MS = 2L
    }

    private enum class Phase { IDLE, WAIT_READY, SENDING, WAIT_DONE }

    private val thread = HandlerThread("rpmloop-file-transfer").apply { start() }
    private val handler = Handler(thread.looper)

    // --- session state (touched only on the transfer thread) ------------------
    private var phase = Phase.IDLE
    private var callback: Callback? = null
    private var data: ByteArray = ByteArray(0)
    private var totalSize = 0L
    private var dataStart = 0L          // resume offset from file_ready
    private var chunkPayload = 16       // recomputed per file from the MTU
    private var totalChunks = 0
    private var nextSeq = 0             // next chunk index to send
    private var lastAckedSeq = -1
    private var lastProgressAtMs = 0L

    private val readyTimeout = Runnable { fail("Device did not answer file_begin") }
    private val doneTimeout = Runnable { fail("Device did not confirm file_end") }
    private val stallWatchdog = object : Runnable {
        override fun run() {
            if (phase != Phase.SENDING) return
            val idleFor = System.currentTimeMillis() - lastProgressAtMs
            if (idleFor > STALL_TIMEOUT_MS) {
                fail("Transfer stalled (no acknowledgement from device)")
            } else {
                handler.postDelayed(this, 2_000L)
            }
        }
    }

    init {
        ble.fileCtrlListener = { json -> handler.post { onCtrlMessage(json) } }
        ble.fileLinkDownListener = {
            handler.post {
                if (phase != Phase.IDLE) fail("Connection lost")
            }
        }
    }

    /** Starts sending [file] to device path [remotePath]. One at a time. */
    fun sendFile(remotePath: String, file: File, cb: Callback) {
        handler.post {
            if (phase != Phase.IDLE) {
                cb.onError("Transfer already in progress")
                return@post
            }
            val bytes = try {
                file.readBytes()
            } catch (e: Exception) {
                cb.onError("Cannot read ${file.name}: ${e.message}")
                return@post
            }
            if (!ble.fileTransferReady()) {
                cb.onError("Device not connected / file transfer not available")
                return@post
            }
            callback = cb
            data = bytes
            totalSize = bytes.size.toLong()
            chunkPayload = (ble.attPayloadSize() - 4).coerceAtLeast(1)
            phase = Phase.WAIT_READY
            lastProgressAtMs = System.currentTimeMillis()
            ble.writeFileCtrl(RpmloopProtocol.buildFileBegin(remotePath, totalSize))
            handler.postDelayed(readyTimeout, READY_TIMEOUT_MS)
        }
    }

    /** Aborts the in-flight file, if any. */
    fun cancel() {
        handler.post {
            if (phase != Phase.IDLE) fail("Cancelled")
        }
    }

    fun isBusy(): Boolean = phase != Phase.IDLE

    // --- device messages -------------------------------------------------------

    private fun onCtrlMessage(json: JSONObject) {
        when (val msg = RpmloopProtocol.parseFileCtrl(json.toString())) {
            is RpmloopProtocol.FileCtrlMessage.Ready -> {
                if (phase != Phase.WAIT_READY) return
                handler.removeCallbacks(readyTimeout)
                dataStart = msg.offset.coerceIn(0L, totalSize)
                val remaining = totalSize - dataStart
                totalChunks = if (remaining <= 0L) {
                    0
                } else {
                    ((remaining + chunkPayload - 1) / chunkPayload).toInt()
                }
                nextSeq = 0
                lastAckedSeq = -1
                lastProgressAtMs = System.currentTimeMillis()
                phase = Phase.SENDING
                handler.post(stallWatchdog)
                if (totalChunks == 0) {
                    sendFileEnd()
                } else {
                    pump()
                }
            }
            is RpmloopProtocol.FileCtrlMessage.Ack -> {
                if (phase != Phase.SENDING) return
                if (msg.seq > lastAckedSeq) {
                    lastAckedSeq = msg.seq
                    lastProgressAtMs = System.currentTimeMillis()
                    reportProgress()
                    if (lastAckedSeq >= totalChunks - 1) {
                        sendFileEnd()
                    } else {
                        pump()
                    }
                }
            }
            is RpmloopProtocol.FileCtrlMessage.Done -> {
                if (phase != Phase.WAIT_DONE) return
                handler.removeCallbacks(doneTimeout)
                val cb = callback
                resetSession()
                cb?.onProgress(100)
                cb?.onComplete()
            }
            is RpmloopProtocol.FileCtrlMessage.Error -> {
                if (phase == Phase.IDLE) return
                fail(msg.reason)
            }
            RpmloopProtocol.FileCtrlMessage.Unknown -> {
                Log.d(TAG, "Ignoring unrelated FILE_CTRL message")
            }
        }
    }

    // --- sending ----------------------------------------------------------------

    /** Sends chunks while the sliding window has room; re-posts itself for
     *  pacing / busy-stack retries. Runs only on the transfer thread. */
    private fun pump() {
        if (phase != Phase.SENDING) return
        while (nextSeq < totalChunks) {
            val unacked = nextSeq - 1 - lastAckedSeq
            if (unacked >= WINDOW) return // wait for acks
            val chunkStart = dataStart + nextSeq.toLong() * chunkPayload
            val chunkEnd = minOf(chunkStart + chunkPayload, totalSize)
            val len = (chunkEnd - chunkStart).toInt()
            val frame = ByteArray(4 + len)
            frame[0] = (nextSeq and 0xFF).toByte()
            frame[1] = ((nextSeq shr 8) and 0xFF).toByte()
            frame[2] = (len and 0xFF).toByte()
            frame[3] = ((len shr 8) and 0xFF).toByte()
            System.arraycopy(data, chunkStart.toInt(), frame, 4, len)
            if (!ble.writeFileDataRaw(frame)) {
                // Stack busy — retry the same chunk shortly.
                handler.postDelayed({ pump() }, WRITE_RETRY_DELAY_MS)
                return
            }
            lastProgressAtMs = System.currentTimeMillis()
            nextSeq++
            if (nextSeq < totalChunks) {
                // Pace writes slightly so the stack / device SD writer keeps up.
                handler.postDelayed({ pump() }, WRITE_PACE_MS)
                return
            }
        }
    }

    private fun sendFileEnd() {
        if (phase != Phase.SENDING) return
        phase = Phase.WAIT_DONE
        handler.removeCallbacks(stallWatchdog)
        ble.writeFileCtrl(RpmloopProtocol.buildFileEnd(totalSize))
        handler.postDelayed(doneTimeout, DONE_TIMEOUT_MS)
    }

    private fun reportProgress() {
        val ackedBytes = if (totalChunks == 0) {
            totalSize
        } else {
            val through = dataStart + (lastAckedSeq + 1).toLong() * chunkPayload
            minOf(through, totalSize)
        }
        val pct = if (totalSize <= 0L) {
            100
        } else {
            (ackedBytes * 100.0 / totalSize).toInt().coerceIn(0, 100)
        }
        callback?.onProgress(pct)
    }

    private fun fail(reason: String) {
        val cb = callback
        resetSession()
        cb?.onError(reason)
    }

    private fun resetSession() {
        phase = Phase.IDLE
        callback = null
        data = ByteArray(0)
        totalSize = 0L
        dataStart = 0L
        totalChunks = 0
        nextSeq = 0
        lastAckedSeq = -1
        handler.removeCallbacks(readyTimeout)
        handler.removeCallbacks(doneTimeout)
        handler.removeCallbacks(stallWatchdog)
    }
}
