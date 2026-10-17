#include "player.h"
#include "../logging.h"
#include <math.h>

PodPlayer *PodPlayer::instance = nullptr;

void PodPlayer::setup() {
  LOG_INFO("=== opentone Audio System Initialization ===");

  if (!audioPlayer.begin()) {
    LOG_ERROR("Couldn't find VS1053, check pin definitions");
    return;
  }
  // Set volume (0-255, lower = louder)
  audioPlayer.setVolume(20, 20);
  LOG_CAT(VS1053, "\n=== VS1053 Register Status ===");
  audioPlayer.dumpRegisters();

  state.addEventListener(onStateEvent);
}

void PodPlayer::onStateEvent(int eventType, void *eventData,
                             EventTarget *source) {
  if (!instance) {
    return;
  }

  switch (eventType) {
  case EVENT_PLAYBACK_STARTED:
    instance->handleTrackPlaybackStarted((PlaybackEvent *)eventData);
    break;
  case EVENT_PLAYBACK_PAUSED:
    instance->handlePlaybackPaused((PlaybackEvent *)eventData);
    break;
  case EVENT_PLAYBACK_RESUMED:
    instance->handlePlaybackResumed((PlaybackEvent *)eventData);
    break;
  case EVENT_PLAYBACK_STOPPED:
    instance->handlePlaybackStopped((PlaybackEvent *)eventData);
    break;
  case EVENT_VOLUME_CHANGED:
    instance->handleVolumeChanged((VolumeEvent *)eventData);
    break;
  default:
    break;
  }
}

void PodPlayer::handleTrackPlaybackStarted(const PlaybackEvent *event) {
  // Silence the old track now, open the new one once the user stops
  // skipping (see TRACK_START_DELAY_MS / loop()). Already silent if a start
  // was pending.
  if (pendingTrackId < 0) {
    audioPlayer.stopPlaying();
  }
  pendingTrackId = event->trackId;
  pendingStartAt = event->startAt;
  pendingArmed = autoAdvancing; // auto-advance: no delay
  pendingTrackAt = millis() - (autoAdvancing ? TRACK_START_DELAY_MS : 0);
  uint32_t indexedDuration =
      musicLookup.getTrackDurationSeconds(event->trackId);
  if (indexedDuration > 0) {
    state.setTrackDuration((int)indexedDuration);
  }
}

void PodPlayer::startPendingTrack() {
  int trackId = pendingTrackId;
  pendingTrackId = -1;
  // The current track's path, then the next one's: Audio_buffer keeps its
  // own copy (or, for the next track, the open file).
  char path[256];
  if (musicLookup.getTrackPath(trackId, path, sizeof(path))) {
    uint32_t indexedDuration = musicLookup.getTrackDurationSeconds(trackId);
    if (audioPlayer.startPlaying(path, indexedDuration)) {
      // Podcast resume: the decoder may not accept a jump yet, so loop()
      // retries for a moment.
      resumeAt = pendingStartAt;
      resumeDeadline = millis() + RESUME_SEEK_TIMEOUT_MS;
      pendingStartAt = 0;
      if (!state.getIsPlaying()) {
        audioPlayer.pausePlaying(true); // paused while the start was pending
      }
      state.setTrackDuration((int)audioPlayer.getDurationSeconds());
    } else {
      playbackFailed = true;
      return; // no point prefetching after a track that won't play
    }
  } else {
    LOG_ERROR("Could not resolve path for track id %d", trackId);
    playbackFailed = true;
    return;
  }

  // Kick off background prefetch of whatever plays next (docs/blog/09-10.md,
  // "Buffers"), so the eventual track change needs less fresh SD
  // activity. If there's no next track (end of list), drop any stale guess
  // left over from a previous playlist/track.
  uint32_t nextTrackId;
  if (state.getNextTrackId(musicLookup, nextTrackId) &&
      musicLookup.getTrackPath(nextTrackId, path, sizeof(path))) {
    uint32_t nextDuration = musicLookup.getTrackDurationSeconds(nextTrackId);
    audioPlayer.prepareNextTrack(path, nextDuration);
  } else {
    audioPlayer.invalidateNextTrack();
  }
}

void PodPlayer::handlePlaybackPaused(const PlaybackEvent *event) {
  audioPlayer.pausePlaying(true);
}

void PodPlayer::handlePlaybackResumed(const PlaybackEvent *event) {
  audioPlayer.resumePlaying();
}

void PodPlayer::handlePlaybackStopped(const PlaybackEvent *event) {
  pendingTrackId = -1;
  resumeAt = 0;
  audioPlayer.stopPlaying();
}

void PodPlayer::handleVolumeChanged(const VolumeEvent *event) {
  // Hardware volume register is attenuation in 0.5 dB steps (0 = loudest,
  // 0xFE = silent). A linear map spreads 0..127 dB across the bar, so only the
  // top quarter was audible. Use an audio taper instead (gain ~ v^2, i.e.
  // 40*log10(v/100) dB): 50% -> -12 dB, 25% -> -24 dB, 10% -> -40 dB, 5% -> -52 dB.
  uint8_t hw = 0xFE;
  if (event->volume > 0) {
    float attenuationDb = 40.0f * log10f(100.0f / (float)event->volume);
    hw = (uint8_t)constrain((int)lroundf(attenuationDb * 2.0f), 0, 0xFE);
  }
  audioPlayer.setVolume(hw, hw);
}

void PodPlayer::loop() {
  if (pendingTrackId >= 0) {
    if (!pendingArmed) {
      pendingArmed = true;
      pendingTrackAt = millis();
    } else if (millis() - pendingTrackAt >= TRACK_START_DELAY_MS) {
      startPendingTrack();
    }
  }
  audioPlayer.loop();
  if (resumeAt > 0) {
    if (audioPlayer.seekToSeconds((uint32_t)resumeAt) ||
        (long)(millis() - resumeDeadline) >= 0) {
      resumeAt = 0;
    }
  }
  if (playbackFailed || audioPlayer.hasFailed()) {
    playbackFailed = false;
    LOG_ERROR("Playback failed, stopping");
    audioPlayer.stopPlaying();
    state.stopPlayback();
    return;
  }
  if (audioPlayer.consumeTrackEnded()) {
    autoAdvancing = true;
    state.notifyTrackEnded(musicLookup);
    autoAdvancing = false;
    if (pendingTrackId >= 0) {
      startPendingTrack(); // keep the gap between tracks minimal
    }
  }
}
