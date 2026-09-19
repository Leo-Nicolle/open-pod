#include "player.h"

PodPlayer *PodPlayer::instance = nullptr;

void PodPlayer::setup() {
  Serial.println("=== OpenPod Audio System Initialization ===");

  if (!audioPlayer.begin()) {
    Serial.println("Couldn't find VS1053, check pin definitions");
    return;
  }
  // Set volume (0-255, lower = louder)
  audioPlayer.setVolume(20, 20);
  Serial.println("\n=== VS1053 Register Status ===");
  audioPlayer.dumpRegisters();
  audioPlayer.sineTest(0x44, 1000); // 1 second test tone

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
  if (musicLookup.getTrackPath(event->trackId, currentPath,
                               sizeof(currentPath))) {
    if (audioPlayer.startPlaying(currentPath)) {
      state.setTrackDuration((int)audioPlayer.getDurationSeconds());
    }
  } else {
    Serial.printf("Could not resolve path for track id %d\n", event->trackId);
  }
}

void PodPlayer::handlePlaybackPaused(const PlaybackEvent *event) {
  audioPlayer.pausePlaying(true);
}

void PodPlayer::handlePlaybackResumed(const PlaybackEvent *event) {
  audioPlayer.resumePlaying();
}

void PodPlayer::handlePlaybackStopped(const PlaybackEvent *event) {
  audioPlayer.stopPlaying();
}

void PodPlayer::handleVolumeChanged(const VolumeEvent *event) {
  // Hardware volume register is inverted: 0 = loudest, 255 = silent.
  uint8_t hw = (uint8_t)constrain(255 - (event->volume * 255 / 100), 0, 255);
  audioPlayer.setVolume(hw, hw);
}

void PodPlayer::loop() {
  audioPlayer.loop();
  if (audioPlayer.consumeTrackEnded()) {
    state.notifyTrackEnded(musicLookup);
  }
}
