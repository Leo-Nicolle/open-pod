#pragma once
#include "AudioPlayer.h"
#include "../pinout.h"
#include "../state/state.h"

// Forward declaration for global state
extern State state;
extern MusicLookup musicLookup;

class PodPlayer {
    void handleTrackPlaybackStarted(const PlaybackEvent* event);
    void handlePlaybackStopped(const PlaybackEvent* event);
    void handlePlaybackPaused(const PlaybackEvent* event);
    void handlePlaybackResumed(const PlaybackEvent* event);
    void handleVolumeChanged(const VolumeEvent* event);
    char currentPath[256]; // Current track path for playback
    // Set when a started track can't be played. Handled from loop() rather
    // than inside the EVENT_PLAYBACK_STARTED dispatch, so the other
    // listeners (UI) see STARTED before the matching STOPPED.
    bool playbackFailed = false;

    // Track changes are applied to the audio after TRACK_START_DELAY_MS
    // without another change, so skipping through several tracks only
    // opens/primes the one landed on (the UI/state still update at once).
    static const unsigned long TRACK_START_DELAY_MS = 200;
    int pendingTrackId = -1;
    int pendingStartAt = 0; // seconds to seek to once started (podcast resume)
    int resumeAt = 0;       // a resume seek still to do (see loop())
    unsigned long resumeDeadline = 0;
    static const unsigned long RESUME_SEEK_TIMEOUT_MS = 2000;
    // The delay is counted from the first loop() pass after the change,
    // not from the change itself: the UI redraw it triggers (album art) can
    // block for longer than the delay, and a click made meanwhile is only
    // handled on the next pass.
    bool pendingArmed = false;
    unsigned long pendingTrackAt = 0;
    bool autoAdvancing = false; // track ended on its own: start with no delay
    void startPendingTrack();

    // Event handler - must be static to use as callback
    static void onStateEvent(int eventType, void *eventData,
                             EventTarget *source);

    // Instance pointer for static callback access
    static PodPlayer *instance;

public:
    // A podcast resume seek is still pending: the decode time doesn't
    // say where playback is yet.
    bool isResuming() const { return resumeAt > 0; }
    AudioPlayer audioPlayer;

    PodPlayer() : audioPlayer(MP3_RESET, MP3CS, MP3XDCS, MP3DREQ, CARDCS) {
        instance = this;
    }

    void setup();
    void loop();
    void printDirectory(File dir, int numTabs);
    void onDataRequest();
};
