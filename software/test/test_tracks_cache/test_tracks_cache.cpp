// test/test_native/test_tracks_cache.cpp
#include <unity.h>
#include "../../src/ui/trackscache.hpp"
#include "../../src/ui/ui_types.h"
#include "../mocks/mock_ui_types.cpp"


// Global test data
TracksCache* cache;
FastFont testFont;
const char* testTracks[TRACKS_PER_SCREEN];

// Unity will automatically call this before each test
void setUp(void) {
    cache = new TracksCache(testFont, 2);
    
    // Initialize test tracks
    testTracks[0] = "Track 1";
    testTracks[1] = "Track 2";
    testTracks[2] = "Track 3";
    testTracks[3] = "Track 4";
    testTracks[4] = "Track 5";
    testTracks[5] = "Track 6";
    testTracks[6] = "Track 7";
    testTracks[7] = "Track 8";
}

// Unity will automatically call this after each test
void tearDown(void) {
    delete cache;
    cache = nullptr;
}

void test_constructor(void) {
    TEST_ASSERT_EQUAL(2, cache->getBitsPerPixel());
}

void test_initial_build(void) {
    cache->build(testTracks);
    
    // Cache should be built successfully
    // We can't directly access internal state, but we can test behavior
    uint16_t* displayBuffer = new uint16_t[SCREEN_WIDTH * TRACK_HEIGHT];
    
    // Try to render each cached track
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
        cache->renderTrackToBuffer(i, false, displayBuffer, SCREEN_WIDTH);
        
        // Buffer should have some non-background content for valid tracks
        bool hasContent = false;
        for (int j = 0; j < SCREEN_WIDTH * TRACK_HEIGHT; j++) {
            if (displayBuffer[j] != COLOR_BACKGROUND) {
                hasContent = true;
                break;
            }
        }
        TEST_ASSERT_TRUE(hasContent);
    }
    
    delete[] displayBuffer;
}

void test_scroll_up(void) {
    // Build initial cache
    cache->build(testTracks);
    
    // Create new tracks for scroll test
    const char* newTracks[TRACKS_PER_SCREEN] = {
        "New Track 1", "Track 1", "Track 2", "Track 3", 
        "Track 4", "Track 5", "Track 6"
    };
    
    // Scroll up by 1
    cache->scrollUp(1, newTracks);
    
    // Verify cache still works
    uint16_t* displayBuffer = new uint16_t[SCREEN_WIDTH * TRACK_HEIGHT];
    cache->renderTrackToBuffer(0, false, displayBuffer, SCREEN_WIDTH);
    
    // Should render without issues
    bool hasContent = false;
    for (int j = 0; j < SCREEN_WIDTH * TRACK_HEIGHT; j++) {
        if (displayBuffer[j] != COLOR_BACKGROUND) {
            hasContent = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(hasContent);
    
    delete[] displayBuffer;
}

void test_scroll_down(void) {
    // Build initial cache
    cache->build(testTracks);
    
    // Create new tracks for scroll test
    const char* newTracks[TRACKS_PER_SCREEN] = {
        "Track 2", "Track 3", "Track 4", "Track 5", 
        "Track 6", "Track 7", "Track 8"
    };
    
    // Scroll down by 1
    cache->scrollDown(1, newTracks);
    
    // Verify cache still works
    uint16_t* displayBuffer = new uint16_t[SCREEN_WIDTH * TRACK_HEIGHT];
    cache->renderTrackToBuffer(TRACKS_PER_SCREEN - 1, false, displayBuffer, SCREEN_WIDTH);
    
    // Should render without issues
    bool hasContent = false;
    for (int j = 0; j < SCREEN_WIDTH * TRACK_HEIGHT; j++) {
        if (displayBuffer[j] != COLOR_BACKGROUND) {
            hasContent = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(hasContent);
    
    delete[] displayBuffer;
}

void test_large_scroll(void) {
    // Build initial cache
    cache->build(testTracks);
    
    // Create completely new tracks
    const char* newTracks[TRACKS_PER_SCREEN] = {
        "New 1", "New 2", "New 3", "New 4", 
        "New 5", "New 6", "New 7"
    };
    
    // Scroll by more than screen size (should trigger rebuild)
    cache->scrollUp(TRACKS_PER_SCREEN, newTracks);
    
    // Verify cache works with completely new content
    uint16_t* displayBuffer = new uint16_t[SCREEN_WIDTH * TRACK_HEIGHT];
    cache->renderTrackToBuffer(0, false, displayBuffer, SCREEN_WIDTH);
    
    bool hasContent = false;
    for (int j = 0; j < SCREEN_WIDTH * TRACK_HEIGHT; j++) {
        if (displayBuffer[j] != COLOR_BACKGROUND) {
            hasContent = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(hasContent);
    
    delete[] displayBuffer;
}

void test_render_selected_vs_unselected(void) {
    cache->build(testTracks);
    
    uint16_t* selectedBuffer = new uint16_t[SCREEN_WIDTH * TRACK_HEIGHT];
    uint16_t* unselectedBuffer = new uint16_t[SCREEN_WIDTH * TRACK_HEIGHT];
    
    // Render same track selected and unselected
    cache->renderTrackToBuffer(0, true, selectedBuffer, SCREEN_WIDTH);
    cache->renderTrackToBuffer(0, false, unselectedBuffer, SCREEN_WIDTH);
    
    // Buffers should be different
    bool areDifferent = false;
    for (int i = 0; i < SCREEN_WIDTH * TRACK_HEIGHT; i++) {
        if (selectedBuffer[i] != unselectedBuffer[i]) {
            areDifferent = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(areDifferent);
    
    delete[] selectedBuffer;
    delete[] unselectedBuffer;
}

void test_invalid_track_index(void) {
    cache->build(testTracks);
    
    uint16_t* displayBuffer = new uint16_t[SCREEN_WIDTH * TRACK_HEIGHT];
    
    // Test negative index
    cache->renderTrackToBuffer(-1, false, displayBuffer, SCREEN_WIDTH);
    
    // Should fill with background
    for (int i = 0; i < SCREEN_WIDTH * TRACK_HEIGHT; i++) {
        TEST_ASSERT_EQUAL(COLOR_BACKGROUND, displayBuffer[i]);
    }
    
    // Test index too large
    cache->renderTrackToBuffer(TRACKS_PER_SCREEN, false, displayBuffer, SCREEN_WIDTH);
    
    // Should still fill with background
    for (int i = 0; i < SCREEN_WIDTH * TRACK_HEIGHT; i++) {
        TEST_ASSERT_EQUAL(COLOR_BACKGROUND, displayBuffer[i]);
    }
    
    delete[] displayBuffer;
}

void test_empty_tracks(void) {
    const char* emptyTracks[TRACKS_PER_SCREEN] = {
        "", "", "", "", "", "", ""
    };
    
    cache->build(emptyTracks);
    
    uint16_t* displayBuffer = new uint16_t[SCREEN_WIDTH * TRACK_HEIGHT];
    cache->renderTrackToBuffer(0, false, displayBuffer, SCREEN_WIDTH);
    
    // Should handle empty tracks gracefully
    // Most pixels should be background color
    int backgroundCount = 0;
    for (int i = 0; i < SCREEN_WIDTH * TRACK_HEIGHT; i++) {
        if (displayBuffer[i] == COLOR_BACKGROUND) {
            backgroundCount++;
        }
    }
    
    // Expect mostly background pixels for empty track
    TEST_ASSERT_GREATER_THAN(SCREEN_WIDTH * TRACK_HEIGHT * 0.9, backgroundCount);
    
    delete[] displayBuffer;
}

void test_null_tracks(void) {
    const char* nullTracks[TRACKS_PER_SCREEN] = {
        nullptr, nullptr, nullptr, nullptr, 
        nullptr, nullptr, nullptr
    };
    
    cache->build(nullTracks);
    
    uint16_t* displayBuffer = new uint16_t[SCREEN_WIDTH * TRACK_HEIGHT];
    cache->renderTrackToBuffer(0, false, displayBuffer, SCREEN_WIDTH);
    
    // Should handle null tracks gracefully - all background
    for (int i = 0; i < SCREEN_WIDTH * TRACK_HEIGHT; i++) {
        TEST_ASSERT_EQUAL(COLOR_BACKGROUND, displayBuffer[i]);
    }
    
    delete[] displayBuffer;
}

void test_update_specific_track(void) {
    cache->build(testTracks);
    
    // Test if track needs update
    TEST_ASSERT_FALSE(cache->needsUpdate(0, "Track 1"));
    TEST_ASSERT_TRUE(cache->needsUpdate(0, "Different Track"));
    TEST_ASSERT_TRUE(cache->needsUpdate(0, nullptr));
    
    // Update a specific track
    cache->updateTrack(0, "Updated Track");
    
    // Verify it renders
    uint16_t* displayBuffer = new uint16_t[SCREEN_WIDTH * TRACK_HEIGHT];
    cache->renderTrackToBuffer(0, false, displayBuffer, SCREEN_WIDTH);
    
    bool hasContent = false;
    for (int j = 0; j < SCREEN_WIDTH * TRACK_HEIGHT; j++) {
        if (displayBuffer[j] != COLOR_BACKGROUND) {
            hasContent = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(hasContent);
    
    delete[] displayBuffer;
}

void test_change_bits_per_pixel(void) {
    cache->build(testTracks);
    
    // Change from 2bpp to 4bpp
    cache->setBitsPerPixel(4);
    TEST_ASSERT_EQUAL(4, cache->getBitsPerPixel());
    
    // Cache should still work after bit depth change
    uint16_t* displayBuffer = new uint16_t[SCREEN_WIDTH * TRACK_HEIGHT];
    
    // Need to rebuild after bit depth change
    cache->build(testTracks);
    cache->renderTrackToBuffer(0, false, displayBuffer, SCREEN_WIDTH);
    
    bool hasContent = false;
    for (int j = 0; j < SCREEN_WIDTH * TRACK_HEIGHT; j++) {
        if (displayBuffer[j] != COLOR_BACKGROUND) {
            hasContent = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(hasContent);
    
    delete[] displayBuffer;
}

void test_invalidate_all(void) {
    cache->build(testTracks);
    cache->invalidateAll();
    
    // After invalidation, rendering should still work but may show background
    uint16_t* displayBuffer = new uint16_t[SCREEN_WIDTH * TRACK_HEIGHT];
    cache->renderTrackToBuffer(0, false, displayBuffer, SCREEN_WIDTH);
    
    // Should render background since cache is invalidated
    int backgroundCount = 0;
    for (int i = 0; i < SCREEN_WIDTH * TRACK_HEIGHT; i++) {
        if (displayBuffer[i] == COLOR_BACKGROUND) {
            backgroundCount++;
        }
    }
    
    // Expect all background pixels after invalidation
    TEST_ASSERT_EQUAL(SCREEN_WIDTH * TRACK_HEIGHT, backgroundCount);
    
    delete[] displayBuffer;
}

// Main test runner
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_constructor);
    RUN_TEST(test_initial_build);
    RUN_TEST(test_scroll_up);
    RUN_TEST(test_scroll_down);
    RUN_TEST(test_large_scroll);
    RUN_TEST(test_render_selected_vs_unselected);
    RUN_TEST(test_invalid_track_index);
    RUN_TEST(test_empty_tracks);
    RUN_TEST(test_null_tracks);
    RUN_TEST(test_update_specific_track);
    RUN_TEST(test_change_bits_per_pixel);
    RUN_TEST(test_invalidate_all);
    
    UNITY_END();
    return 0;
}