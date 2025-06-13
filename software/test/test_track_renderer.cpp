// test/test_native/test_track_renderer.cpp
#include <doctest.h>
#include "../../src/ui/trackrenderer.hpp"
#include "./mock_ui_types.h"

class TrackRendererTestFixture {
public:
    TrackRenderer* renderer;
    uint8_t* testBuffer;
    int bufferSize;
    
    TrackRendererTestFixture() {
        // Test with 2 bits per pixel
        renderer = new TrackRenderer(IBMPlexSans16Bold, 2);
        
        // Allocate test buffer
        int width = 100;
        int height = 30;
        bufferSize = TrackRenderer::calculateBufferSize(width, height, 2);
        testBuffer = new uint8_t[bufferSize];
        memset(testBuffer, 0, bufferSize);
    }
    
    ~TrackRendererTestFixture() {
        delete renderer;
        delete[] testBuffer;
    }
};

TEST_CASE_FIXTURE(TrackRendererTestFixture, "Constructor") {
    CHECK_EQ(2, renderer->getBitsPerPixel());
}

TEST_CASE_FIXTURE(TrackRendererTestFixture, "Buffer size calculation") {
    SUBCASE("1 bit per pixel") {
        int size1 = TrackRenderer::calculateBufferSize(100, 30, 1);
        CHECK_EQ((100 * 30 + 7) / 8, size1);
    }
    
    SUBCASE("2 bits per pixel") {
        int size2 = TrackRenderer::calculateBufferSize(100, 30, 2);
        CHECK_EQ((100 * 30 * 2 + 7) / 8, size2);
    }
    
    SUBCASE("4 bits per pixel") {
        int size4 = TrackRenderer::calculateBufferSize(100, 30, 4);
        CHECK_EQ((100 * 30 * 4 + 7) / 8, size4);
    }
}

TEST_CASE_FIXTURE(TrackRendererTestFixture, "Bits per pixel validation") {
    SUBCASE("Valid values") {
        renderer->setBitsPerPixel(1);
        CHECK_EQ(1, renderer->getBitsPerPixel());
        
        renderer->setBitsPerPixel(2);
        CHECK_EQ(2, renderer->getBitsPerPixel());
        
        renderer->setBitsPerPixel(4);
        CHECK_EQ(4, renderer->getBitsPerPixel());
    }
    
    SUBCASE("Invalid value should not change") {
        renderer->setBitsPerPixel(2); // Set to known good value
        renderer->setBitsPerPixel(3); // Try invalid
        CHECK_EQ(2, renderer->getBitsPerPixel()); // Should remain 2
    }
}

TEST_CASE_FIXTURE(TrackRendererTestFixture, "Binary render basic") {
    const char* testText = "Test Track";
    int width = 100;
    int height = 30;
    
    renderer->renderTrackBinary(testText, testBuffer, width, height, 0, width);
    
    // Buffer should not be all zeros after rendering
    bool hasNonZero = false;
    for (int i = 0; i < bufferSize; i++) {
        if (testBuffer[i] != 0) {
            hasNonZero = true;
            break;
        }
    }
    CHECK(hasNonZero);
}

TEST_CASE_FIXTURE(TrackRendererTestFixture, "Binary pixel operations") {
    // Test setting and getting pixels for different bit depths
    TrackRenderer renderer1(IBMPlexSans16Bold, 1);
    TrackRenderer renderer2(IBMPlexSans16Bold, 2);
    TrackRenderer renderer4(IBMPlexSans16Bold, 4);
    
    // Create a small test binary buffer
    int testWidth = 8;
    int testHeight = 4;
    int testSize = TrackRenderer::calculateBufferSize(testWidth, testHeight, 2);
    uint8_t* binaryTest = new uint8_t[testSize];
    memset(binaryTest, 0, testSize);
    
    // Create display buffer
    uint16_t* displayBuffer = new uint16_t[testWidth * testHeight];
    
    // Test rendering binary to display
    renderer->renderBinaryToDisplay(binaryTest, testWidth, testHeight,
                                   displayBuffer, testWidth, testHeight,
                                   COLOR_TEXT, COLOR_BACKGROUND, false);
    
    // All pixels should be background color since binary buffer is empty
    for (int i = 0; i < testWidth * testHeight; i++) {
        CHECK_EQ(COLOR_BACKGROUND, displayBuffer[i]);
    }
    
    delete[] binaryTest;
    delete[] displayBuffer;
}

TEST_CASE_FIXTURE(TrackRendererTestFixture, "Color blending") {
    // Test color blending by rendering a pattern and checking results
    uint16_t* displayBuffer = new uint16_t[100 * 30];
    uint8_t* binaryBuffer = new uint8_t[TrackRenderer::calculateBufferSize(100, 30, 2)];
    
    // Fill binary buffer with different transparency values
    memset(binaryBuffer, 0x55, TrackRenderer::calculateBufferSize(100, 30, 2)); // Pattern
    
    renderer->renderBinaryToDisplay(binaryBuffer, 100, 30,
                                   displayBuffer, 100, 30,
                                   COLOR_TEXT, COLOR_BACKGROUND, false);
    
    // Check that we get blended colors (not just background or text)
    bool hasBlendedColors = false;
    for (int i = 0; i < 100 * 30; i++) {
        if (displayBuffer[i] != COLOR_BACKGROUND && displayBuffer[i] != COLOR_TEXT) {
            hasBlendedColors = true;
            break;
        }
    }
    CHECK(hasBlendedColors);
    
    delete[] displayBuffer;
    delete[] binaryBuffer;
}

TEST_CASE_FIXTURE(TrackRendererTestFixture, "Gradient effect") {
    uint16_t* displayBuffer = new uint16_t[100 * 30];
    uint8_t* binaryBuffer = new uint8_t[TrackRenderer::calculateBufferSize(100, 30, 2)];
    
    // Fill with maximum transparency to test gradient
    memset(binaryBuffer, 0xFF, TrackRenderer::calculateBufferSize(100, 30, 2));
    
    // Test with selected = true to enable gradient
    renderer->renderBinaryToDisplay(binaryBuffer, 100, 30,
                                   displayBuffer, 100, 30,
                                   COLOR_BACKGROUND, COLOR_PRIMARY, true);
    
    // Check that different rows have different colors (gradient effect)
    uint16_t firstRowColor = displayBuffer[0];
    uint16_t lastRowColor = displayBuffer[99 + (29 * 100)]; // Last row, first pixel
    
    // Due to gradient, these should be different
    CHECK_NE(firstRowColor, lastRowColor);
    
    delete[] displayBuffer;
    delete[] binaryBuffer;
}

TEST_CASE_FIXTURE(TrackRendererTestFixture, "Empty text rendering") {
    const char* emptyText = "";
    int width = 100;
    int height = 30;
    
    renderer->renderTrackBinary(emptyText, testBuffer, width, height, 0, width);
    
    // Buffer should be mostly zeros for empty text
    int nonZeroCount = 0;
    for (int i = 0; i < bufferSize; i++) {
        if (testBuffer[i] != 0) nonZeroCount++;
    }
    
    // Allow some non-zero bytes due to potential initialization, but should be minimal
    CHECK_LT(nonZeroCount, bufferSize / 10);
}

TEST_CASE_FIXTURE(TrackRendererTestFixture, "Long text rendering") {
    const char* longText = "This is a very long track name that should be clipped or handled properly by the renderer";
    int width = 100;
    int height = 30;
    
    renderer->renderTrackBinary(longText, testBuffer, width, height, 0, width);
    
    // Should not crash and should produce some output
    bool hasContent = false;
    for (int i = 0; i < bufferSize; i++) {
        if (testBuffer[i] != 0) {
            hasContent = true;
            break;
        }
    }
    CHECK(hasContent);
}

TEST_CASE_FIXTURE(TrackRendererTestFixture, "Offset rendering") {
    const char* testText = "Offset Test";
    int width = 50;
    int height = 30;
    int offset = 25; // Start rendering from middle
    
    renderer->renderTrackBinary(testText, testBuffer, width, height, offset, width);
    
    // Should still produce output even with offset
    bool hasContent = false;
    for (int i = 0; i < bufferSize; i++) {
        if (testBuffer[i] != 0) {
            hasContent = true;
            break;
        }
    }
    CHECK(hasContent);
}