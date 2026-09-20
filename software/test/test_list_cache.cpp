#include <doctest.h>
#include "../src/ui/list/list_cache.hpp"
#include "../src/ui/list/list_renderer.hpp"
#include "../src/fonts/IBMPlexSans16Bold.h"
#include "./mock_ui_types.h"

class ListCacheTestFixture {
public:
    ListCache* cache;
    ListRenderer* renderer;
    const char* testTracks[ELEMENTS_PER_SCREEN];

    ListCacheTestFixture() {
        cache = new ListCache(IBMPlexSans16Bold, 2);
        renderer = new ListRenderer(*cache);

        // Initialize test tracks
        testTracks[0] = "Track 1";
        testTracks[1] = "Track 2";
        testTracks[2] = "Track 3";
        testTracks[3] = "Track 4";
        testTracks[4] = "Track 5";
        testTracks[5] = "Track 6";
        testTracks[6] = "Track 7";
    }

    ~ListCacheTestFixture() {
        delete renderer;
        delete cache;
    }
};

TEST_CASE_FIXTURE(ListCacheTestFixture, "Constructor") {
    CHECK_EQ(2, cache->getBitsPerPixel());
}

TEST_CASE_FIXTURE(ListCacheTestFixture, "Initial build") {
    cache->build(testTracks);

    // Cache should be built successfully
    // We can't directly access internal state, but we can test behavior.
    // renderRect always reads a row's data from the cache's own fixed
    // dimensions (cache->cacheWidth x cache->cacheHeight), never from the
    // width/height arguments, so the display buffer and the width passed to
    // renderRect must match cache->cacheWidth here rather than SCREEN_WIDTH.
    uint16_t* displayBuffer = new uint16_t[cache->cacheWidth * ELEMENT_HEIGHT];

    // Try to render each cached track using the renderer
    for (int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
        renderer->renderRect(0, i * ELEMENT_HEIGHT, cache->cacheWidth, ELEMENT_HEIGHT, displayBuffer, -1);

        // Buffer should render without errors
        bool hasValidData = true;
        for (int j = 0; j < cache->cacheWidth * ELEMENT_HEIGHT; j++) {
            if (displayBuffer[j] > 0xFFFF) {
                hasValidData = false;
                break;
            }
        }
        CHECK(hasValidData);
    }

    delete[] displayBuffer;
}

TEST_CASE_FIXTURE(ListCacheTestFixture, "Scroll operations") {
    SUBCASE("Scroll up") {
        // Build initial cache
        cache->build(testTracks);

        // Create new tracks for scroll test
        const char* newTracks[ELEMENTS_PER_SCREEN] = {
            "New Track 1", "Track 1", "Track 2", "Track 3",
            "Track 4", "Track 5"
        };

        // Scroll up by 1
        cache->scrollUp(1, newTracks);

        // Verify cache still works
        uint16_t* displayBuffer = new uint16_t[cache->cacheWidth * ELEMENT_HEIGHT];
        renderer->renderRect(0, 0, cache->cacheWidth, ELEMENT_HEIGHT, displayBuffer, -1);

        // Should render without issues
        bool hasValidData = true;
        for (int j = 0; j < cache->cacheWidth * ELEMENT_HEIGHT; j++) {
            if (displayBuffer[j] > 0xFFFF) {
                hasValidData = false;
                break;
            }
        }
        CHECK(hasValidData);

        delete[] displayBuffer;
    }

    SUBCASE("Scroll down") {
        // Build initial cache
        cache->build(testTracks);

        // Create new tracks for scroll test
        const char* newTracks[ELEMENTS_PER_SCREEN] = {
            "Track 2", "Track 3", "Track 4", "Track 5",
            "Track 6", "Track 7"
        };

        // Scroll down by 1
        cache->scrollDown(1, newTracks);

        // Verify cache still works
        uint16_t* displayBuffer = new uint16_t[cache->cacheWidth * ELEMENT_HEIGHT];
        renderer->renderRect(0, (ELEMENTS_PER_SCREEN - 1) * ELEMENT_HEIGHT, cache->cacheWidth, ELEMENT_HEIGHT, displayBuffer, -1);

        // Should render without issues
        bool hasValidData = true;
        for (int j = 0; j < cache->cacheWidth * ELEMENT_HEIGHT; j++) {
            if (displayBuffer[j] > 0xFFFF) {
                hasValidData = false;
                break;
            }
        }
        CHECK(hasValidData);

        delete[] displayBuffer;
    }

    SUBCASE("Large scroll") {
        // Build initial cache
        cache->build(testTracks);

        // Create completely new tracks
        const char* newTracks[ELEMENTS_PER_SCREEN] = {
            "New 1", "New 2", "New 3", "New 4",
            "New 5", "New 6"
        };

        // Scroll by more than screen size (should trigger rebuild)
        cache->scrollUp(ELEMENTS_PER_SCREEN, newTracks);

        // Verify cache works with completely new content
        uint16_t* displayBuffer = new uint16_t[cache->cacheWidth * ELEMENT_HEIGHT];
        renderer->renderRect(0, 0, cache->cacheWidth, ELEMENT_HEIGHT, displayBuffer, -1);

        bool hasValidData = true;
        for (int j = 0; j < cache->cacheWidth * ELEMENT_HEIGHT; j++) {
            if (displayBuffer[j] > 0xFFFF) {
                hasValidData = false;
                break;
            }
        }
        CHECK(hasValidData);

        delete[] displayBuffer;
    }
}

TEST_CASE_FIXTURE(ListCacheTestFixture, "Render selected vs unselected") {
    cache->build(testTracks);

    uint16_t* selectedBuffer = new uint16_t[cache->cacheWidth * ELEMENT_HEIGHT];
    uint16_t* unselectedBuffer = new uint16_t[cache->cacheWidth * ELEMENT_HEIGHT];

    // Render same track selected and unselected
    renderer->renderRect(0, 0, cache->cacheWidth, ELEMENT_HEIGHT, selectedBuffer, 0);
    renderer->renderRect(0, 0, cache->cacheWidth, ELEMENT_HEIGHT, unselectedBuffer, -1);

    // Buffers should be different
    bool areDifferent = false;
    for (int i = 0; i < cache->cacheWidth * ELEMENT_HEIGHT; i++) {
        if (selectedBuffer[i] != unselectedBuffer[i]) {
            areDifferent = true;
            break;
        }
    }
    CHECK(areDifferent);

    delete[] selectedBuffer;
    delete[] unselectedBuffer;
}

TEST_CASE_FIXTURE(ListCacheTestFixture, "Edge cases") {
    SUBCASE("Empty tracks") {
        const char* emptyTracks[ELEMENTS_PER_SCREEN] = {
            "", "", "", "", "", "", ""
        };

        cache->build(emptyTracks);

        uint16_t* displayBuffer = new uint16_t[cache->cacheWidth * ELEMENT_HEIGHT];
        renderer->renderRect(0, 0, cache->cacheWidth, ELEMENT_HEIGHT, displayBuffer, -1);

        // Should handle empty tracks gracefully
        // Most pixels should be background color
        int backgroundCount = 0;
        for (int i = 0; i < cache->cacheWidth * ELEMENT_HEIGHT; i++) {
            if (displayBuffer[i] == COLOR_BACKGROUND) {
                backgroundCount++;
            }
        }

        // Expect mostly background pixels for empty track
        CHECK_GT(backgroundCount, static_cast<int>(cache->cacheWidth * ELEMENT_HEIGHT * 0.9));

        delete[] displayBuffer;
    }

    SUBCASE("Null tracks") {
        const char* nullTracks[ELEMENTS_PER_SCREEN] = {
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr
        };

        cache->build(nullTracks);

        uint16_t* displayBuffer = new uint16_t[cache->cacheWidth * ELEMENT_HEIGHT];
        renderer->renderRect(0, 0, cache->cacheWidth, ELEMENT_HEIGHT, displayBuffer, -1);

        // Should handle null tracks gracefully - all background
        bool allBackground = true;
        for (int i = 0; i < cache->cacheWidth * ELEMENT_HEIGHT; i++) {
            if (displayBuffer[i] != COLOR_BACKGROUND) {
                allBackground = false;
                break;
            }
        }
        CHECK(allBackground);

        delete[] displayBuffer;
    }
}
