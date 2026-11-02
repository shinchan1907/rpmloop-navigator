package io.bytenex.rpmloop.media

/**
 * TODO(Phase 2): Phone media-session control.
 *
 * Planned: MediaSessionManager (via a NotificationListenerService) reads the
 * active media session — title, artist, album, playback position — and sends
 * "media" messages on the media_state characteristic, plus album art
 * (<=120x120 JPEG, chunked) on media_art. Device commands arriving on the
 * device_cmd characteristic ("media_cmd": play/pause/next/previous) are
 * forwarded to the session's transport controls.
 *
 * Intentionally empty in Phase 1. BleManager already receives and logs
 * device_cmd notifications; nothing is acted on yet.
 */
class MediaSessionController {
    // TODO(Phase 2): implement media session read + device command handling.
}
