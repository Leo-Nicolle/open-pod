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
    char currentPath[256]; // Current track path for playback

    // Event handler - must be static to use as callback
    static void onStateEvent(int eventType, void *eventData,
                             EventTarget *source);

    // Instance pointer for static callback access
    static PodPlayer *instance;

public:
    AudioPlayer audioPlayer;

    PodPlayer() : audioPlayer(MP3_RESET, MP3CS, MP3XDCS, MP3DREQ, CARDCS) {
        instance = this;
    }

    void setup();
    void loop();
    void printDirectory(File dir, int numTabs);
    void onDataRequest();
};
