package io.bytenex.rpmloop.tiles

import io.bytenex.rpmloop.ble.BleManager
import io.bytenex.rpmloop.ble.FileTransferClient
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/** Aggregate progress of a "Send tiles to device" run. */
data class TransferProgress(
    val running: Boolean = false,
    val filesDone: Int = 0,
    val filesTotal: Int = 0,
    val currentFile: String? = null,
    val currentFilePct: Int = 0,
    val approxKBps: Int = 0,
    val finished: Boolean = false,
    val message: String? = null
)

/**
 * Sends every downloaded-but-not-yet-sent tile to the device, one file at a
 * time through [FileTransferClient], and records confirmations in the
 * TileStore sent-manifest.
 *
 * Policy (product spec): a file that fails with file_error (or a transfer
 * error) is retried ONCE, then skipped and reported in the final message.
 * A disconnect aborts the whole run — the unsent files simply stay pending
 * and resume (via the device's file_ready offset) on the next run.
 */
class TileTransferManager(ble: BleManager, private val store: TileStore) {

    private val client = FileTransferClient(ble)

    @Volatile
    private var running = false

    @Volatile
    private var cancelled = false

    val isRunning: Boolean get() = running

    /**
     * Runs the transfer on the CALLING thread (call from a background
     * thread). [onProgress] is invoked from this thread.
     */
    fun sendPending(onProgress: (TransferProgress) -> Unit) {
        if (running) {
            onProgress(TransferProgress(message = "A transfer is already running"))
            return
        }
        val pending = store.pendingTiles()
        if (pending.isEmpty()) {
            onProgress(
                TransferProgress(
                    finished = true,
                    message = "Nothing to send — all downloaded tiles are already on the device"
                )
            )
            return
        }
        running = true
        cancelled = false
        val startedAt = System.currentTimeMillis()
        var bytesConfirmed = 0L
        var done = 0
        val skipped = ArrayList<String>()
        try {
            for (coord in pending) {
                if (cancelled) break
                val file = store.localFile(coord)
                onProgress(
                    TransferProgress(
                        running = true,
                        filesDone = done,
                        filesTotal = pending.size,
                        currentFile = coord.devicePath(),
                        currentFilePct = 0,
                        approxKBps = rateKBps(bytesConfirmed, startedAt)
                    )
                )
                val outcome = transferOne(coord, file, onProgress, done, pending.size, startedAt) {
                    bytesConfirmed
                }
                when (outcome) {
                    Outcome.SENT -> {
                        store.markSent(coord)
                        bytesConfirmed += file.length()
                        done++
                    }
                    Outcome.SKIPPED -> {
                        skipped.add(coord.devicePath())
                        done++
                    }
                    Outcome.ABORTED -> {
                        onProgress(
                            TransferProgress(
                                filesDone = done,
                                filesTotal = pending.size,
                                finished = true,
                                message = "Transfer stopped — remaining tiles stay pending " +
                                    "and resume next time"
                            )
                        )
                        return
                    }
                }
                onProgress(
                    TransferProgress(
                        running = true,
                        filesDone = done,
                        filesTotal = pending.size,
                        approxKBps = rateKBps(bytesConfirmed, startedAt)
                    )
                )
            }
            val msg = buildString {
                append("Sent $done of ${pending.size} tiles")
                if (skipped.isNotEmpty()) {
                    append(" · skipped ${skipped.size} after a retry: ")
                    append(skipped.take(3).joinToString(", "))
                    if (skipped.size > 3) append(" …")
                }
                if (cancelled) append(" · cancelled")
            }
            onProgress(
                TransferProgress(
                    filesDone = done,
                    filesTotal = pending.size,
                    finished = true,
                    approxKBps = rateKBps(bytesConfirmed, startedAt),
                    message = msg
                )
            )
        } finally {
            running = false
        }
    }

    fun cancel() {
        cancelled = true
        client.cancel()
    }

    private enum class Outcome { SENT, SKIPPED, ABORTED }

    /** One file, up to 2 attempts (initial + one retry). */
    private fun transferOne(
        coord: TileCoord,
        file: java.io.File,
        onProgress: (TransferProgress) -> Unit,
        filesDone: Int,
        filesTotal: Int,
        startedAt: Long,
        confirmedBytes: () -> Long
    ): Outcome {
        var attempt = 0
        while (attempt < 2) {
            attempt++
            if (cancelled) return Outcome.ABORTED
            val latch = CountDownLatch(1)
            var success = false
            var error: String? = null
            client.sendFile(coord.devicePath(), file, object : FileTransferClient.Callback {
                override fun onProgress(percent: Int) {
                    onProgress(
                        TransferProgress(
                            running = true,
                            filesDone = filesDone,
                            filesTotal = filesTotal,
                            currentFile = coord.devicePath(),
                            currentFilePct = percent,
                            approxKBps = rateKBps(confirmedBytes(), startedAt)
                        )
                    )
                }

                override fun onComplete() {
                    success = true
                    latch.countDown()
                }

                override fun onError(reason: String) {
                    error = reason
                    latch.countDown()
                }
            })
            // Generous ceiling: the client's own timeouts fire long before.
            latch.await(5, TimeUnit.MINUTES)
            if (success) return Outcome.SENT
            val reason = error ?: "timeout"
            if (cancelled || reason.contains("Connection lost", ignoreCase = true) ||
                reason.contains("Cancelled", ignoreCase = true)
            ) {
                return Outcome.ABORTED
            }
            // else: fall through to the single retry, then skip.
        }
        return Outcome.SKIPPED
    }

    private fun rateKBps(bytes: Long, startedAt: Long): Int {
        val elapsedS = (System.currentTimeMillis() - startedAt) / 1000.0
        if (elapsedS <= 0.0 || bytes <= 0L) return 0
        return (bytes / 1024.0 / elapsedS).toInt()
    }
}
