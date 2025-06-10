// test/test_native/test_track_renderer.cpp
#include <unity.h>
#include "../../src/ui/trackrenderer.hpp"
#include "../mocks/mock_ui_types.cpp"
class TrackRendererTest {
private:
    TrackRenderer* renderer;
    FastFont testFont;
    uint8_t* testBuffer;
    int bufferSize;
    
public:
    void setUp() {
        // Test with 2 bits per pixel
        renderer = new TrackRenderer(testFont, 2);
        
        // Allocate test buffer
        int width = 100;
        int height = 30;
        bufferSize = TrackRenderer::calculateBufferSize(width, height, 2);
        testBuffer = new uint8_t[bufferSize];
        memset(testBuffer, 0, bufferSize);
    }
    
    void tearDown() {
        delete renderer;
        delete[] testBuffer;
    }
    
    void test_constructor() {
        TEST_ASSERT_EQUAL(2, renderer->getBitsPerPixel());
    }
    
    void test_buffer_size_calculation() {
        // Test 1 bit per pixel
        int size1 = TrackRenderer::calculateBufferSize(100, 30, 1);
        TEST_ASSERT_EQUAL((100 * 30 + 7) / 8, size1);
        
        // Test 2 bits per pixel
        int size2 = TrackRenderer::calculateBufferSize(100, 30, 2);
        TEST_ASSERT_EQUAL((100 * 30 * 2 + 7) / 8, size2);
        
        // Test 4 bits per pixel
        int size4 = TrackRenderer::calculateBufferSize(100, 30, 4);
        TEST_ASSERT_EQUAL((100 * 30 * 4 + 7) / 8, size4);
    }
    
    void test_bits_per_pixel_validation() {
        // Valid values
        renderer->setBitsPerPixel(1);
        TEST_ASSERT_EQUAL(1, renderer->getBitsPerPixel());
        
        renderer->setBitsPerPixel(2);
        TEST_ASSERT_EQUAL(2, renderer->getBitsPerPixel());
        
        renderer->setBitsPerPixel(4);
        TEST_ASSERT_EQUAL(4, renderer->getBitsPerPixel());
        
        // Invalid value should not change
        renderer->setBitsPerPixel(2); // Set to known good value
        renderer->setBitsPerPixel(3); // Try invalid
        TEST_ASSERT_EQUAL(2, renderer->getBitsPerPixel()); // Should remain 2
    }
    
    void test_binary_render_basic() {
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
        TEST_ASSERT_TRUE(hasNonZero);
    }
    
    void test_binary_pixel_operations() {
        // Test setting and getting pixels for different bit depths
        TrackRenderer renderer1(testFont, 1);
        TrackRenderer renderer2(testFont, 2);
        TrackRenderer renderer4(testFont, 4);
        
        uint8_t buffer[16]; // Enough for small test
        memset(buffer, 0, sizeof(buffer));
        
        // Test 1bpp - access private methods through public interface
        // We'll test the overall functionality through renderBinaryToDisplay
        
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
            TEST_ASSERT_EQUAL(COLOR_BACKGROUND, displayBuffer[i]);
        }
        
        delete[] binaryTest;
        delete[] displayBuffer;
    }
    
    void test_color_blending() {
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
        TEST_ASSERT_TRUE(hasBlendedColors);
        
        delete[] displayBuffer;
        delete[] binaryBuffer;
    }
    
    void test_gradient_effect() {
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
        TEST_ASSERT_NOT_EQUAL(firstRowColor, lastRowColor);
        
        delete[] displayBuffer;
        delete[] binaryBuffer;
    }
    
    void test_empty_text_rendering() {
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
        TEST_ASSERT_LESS_THAN(bufferSize / 10, nonZeroCount);
    }
    
    void test_long_text_rendering() {
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
        TEST_ASSERT_TRUE(hasContent);
    }
    
    void test_offset_rendering() {
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
        TEST_ASSERT_TRUE(hasContent);
    }
};

// Global test instance
TrackRendererTest rendererTestInstance;

void setUp(void) {
    rendererTestInstance.setUp();
}

void tearDownRenderer(void) {
    rendererTestInstance.tearDown();
}

void test_renderer_constructor(void) {
    rendererTestInstance.test_constructor();
}

void test_renderer_buffer_size_calculation(void) {
    rendererTestInstance.test_buffer_size_calculation();
}

void test_renderer_bits_per_pixel_validation(void) {
    rendererTestInstance.test_bits_per_pixel_validation();
}

void test_renderer_binary_render_basic(void) {
    rendererTestInstance.test_binary_render_basic();
}

void test_renderer_binary_pixel_operations(void) {
    rendererTestInstance.test_binary_pixel_operations();
}

void test_renderer_color_blending(void) {
    rendererTestInstance.test_color_blending();
}

void test_renderer_gradient_effect(void) {
    rendererTestInstance.test_gradient_effect();
}

void test_renderer_empty_text_rendering(void) {
    rendererTestInstance.test_empty_text_rendering();
}

void test_renderer_long_text_rendering(void) {
    rendererTestInstance.test_long_text_rendering();
}

void test_renderer_offset_rendering(void) {
    rendererTestInstance.test_offset_rendering();
}

void runRendererTests() {
    UNITY_BEGIN();
    
    RUN_TEST(test_renderer_constructor);
    RUN_TEST(test_renderer_buffer_size_calculation);
    RUN_TEST(test_renderer_bits_per_pixel_validation);
    RUN_TEST(test_renderer_binary_render_basic);
    RUN_TEST(test_renderer_binary_pixel_operations);
    RUN_TEST(test_renderer_color_blending);
    RUN_TEST(test_renderer_gradient_effect);
    RUN_TEST(test_renderer_empty_text_rendering);
    RUN_TEST(test_renderer_long_text_rendering);
    RUN_TEST(test_renderer_offset_rendering);
    
    UNITY_END();
}


// Main test runner
int main(int argc, char **argv) {
    runRendererTests();
    return 0;
}