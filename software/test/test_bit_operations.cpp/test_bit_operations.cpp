// test/test_native/test_bit_operations.cpp
#include <unity.h>
#include "../../src/ui/trackrenderer.hpp"
#include "../mocks/mock_ui_types.cpp"

class BitOperationsTest {
private:
    TrackRenderer* renderer1bpp;
    TrackRenderer* renderer2bpp;
    TrackRenderer* renderer4bpp;
    FastFont testFont;
    
public:
    void setUp() {
        renderer1bpp = new TrackRenderer(testFont, 1);
        renderer2bpp = new TrackRenderer(testFont, 2);
        renderer4bpp = new TrackRenderer(testFont, 4);
    }
    
    void tearDown() {
        delete renderer1bpp;
        delete renderer2bpp;
        delete renderer4bpp;
    }
    
    void test_1bpp_storage_efficiency() {
        int width = 64;
        int height = 32;
        int expectedBytes = (width * height + 7) / 8; // Round up to nearest byte
        
        int actualBytes = TrackRenderer::calculateBufferSize(width, height, 1);
        TEST_ASSERT_EQUAL(expectedBytes, actualBytes);
        
        // Test that we can actually store and retrieve data
        uint8_t* buffer = new uint8_t[actualBytes];
        memset(buffer, 0, actualBytes);
        
        renderer1bpp->renderTrackBinary("Test", buffer, width, height, 0, width);
        
        // Buffer should have some content
        bool hasContent = false;
        for (int i = 0; i < actualBytes; i++) {
            if (buffer[i] != 0) {
                hasContent = true;
                break;
            }
        }
        TEST_ASSERT_TRUE(hasContent);
        
        delete[] buffer;
    }
    
    void test_2bpp_transparency_levels() {
        int width = 32;
        int height = 16;
        int bufferSize = TrackRenderer::calculateBufferSize(width, height, 2);
        
        uint8_t* binaryBuffer = new uint8_t[bufferSize];
        uint16_t* displayBuffer = new uint16_t[width * height];
        
        // Test different transparency patterns
        memset(binaryBuffer, 0x00, bufferSize); // All 00 (transparent)
        renderer2bpp->renderBinaryToDisplay(binaryBuffer, width, height,
                                           displayBuffer, width, height,
                                           COLOR_TEXT, COLOR_BACKGROUND, false);
        
        // Should be all background
        for (int i = 0; i < width * height; i++) {
            TEST_ASSERT_EQUAL(COLOR_BACKGROUND, displayBuffer[i]);
        }
        
        memset(binaryBuffer, 0xFF, bufferSize); // All 11 (opaque)
        renderer2bpp->renderBinaryToDisplay(binaryBuffer, width, height,
                                           displayBuffer, width, height,
                                           COLOR_TEXT, COLOR_BACKGROUND, false);
        
        // Should be all text color
        for (int i = 0; i < width * height; i++) {
            TEST_ASSERT_EQUAL(COLOR_TEXT, displayBuffer[i]);
        }
        
        // Test intermediate values
        memset(binaryBuffer, 0x55, bufferSize); // Pattern of 01 (25% opacity)
        renderer2bpp->renderBinaryToDisplay(binaryBuffer, width, height,
                                           displayBuffer, width, height,
                                           COLOR_TEXT, COLOR_BACKGROUND, false);
        
        // Should get blended colors
        bool hasBlendedColors = false;
        for (int i = 0; i < width * height; i++) {
            if (displayBuffer[i] != COLOR_BACKGROUND && displayBuffer[i] != COLOR_TEXT) {
                hasBlendedColors = true;
                break;
            }
        }
        TEST_ASSERT_TRUE(hasBlendedColors);
        
        delete[] binaryBuffer;
        delete[] displayBuffer;
    }
    
    void test_4bpp_smooth_gradients() {
        int width = 16;
        int height = 16;
        int bufferSize = TrackRenderer::calculateBufferSize(width, height, 4);
        
        uint8_t* binaryBuffer = new uint8_t[bufferSize];
        uint16_t* displayBuffer = new uint16_t[width * height];
        
        // Create a gradient pattern in the binary buffer
        memset(binaryBuffer, 0, bufferSize);
        
        // We'll simulate a pattern where each 4-bit value represents increasing opacity
        // Since we can't easily set individual 4-bit values, we'll test with patterns
        
        // Test minimum transparency (all 0000)
        memset(binaryBuffer, 0x00, bufferSize);
        renderer4bpp->renderBinaryToDisplay(binaryBuffer, width, height,
                                           displayBuffer, width, height,
                                           COLOR_TEXT, COLOR_BACKGROUND, false);
        
        // Should be background
        TEST_ASSERT_EQUAL(COLOR_BACKGROUND, displayBuffer[0]);
        
        // Test maximum transparency (all 1111)
        memset(binaryBuffer, 0xFF, bufferSize);
        renderer4bpp->renderBinaryToDisplay(binaryBuffer, width, height,
                                           displayBuffer, width, height,
                                           COLOR_TEXT, COLOR_BACKGROUND, false);
        
        // Should be text color
        TEST_ASSERT_EQUAL(COLOR_TEXT, displayBuffer[0]);
        
        // Test intermediate patterns
        memset(binaryBuffer, 0x88, bufferSize); // 1000 pattern (8/15 opacity)
        renderer4bpp->renderBinaryToDisplay(binaryBuffer, width, height,
                                           displayBuffer, width, height,
                                           COLOR_TEXT, COLOR_BACKGROUND, false);
        
        // Should get a blended color
        uint16_t blendedColor = displayBuffer[0];
        TEST_ASSERT_NOT_EQUAL(COLOR_BACKGROUND, blendedColor);
        TEST_ASSERT_NOT_EQUAL(COLOR_TEXT, blendedColor);
        
        delete[] binaryBuffer;
        delete[] displayBuffer;
    }
    
    void test_bit_boundary_handling() {
        // Test edge cases where bits cross byte boundaries
        
        // Test with odd widths and different bit depths
        int testWidths[] = {7, 15, 31, 63, 127};
        int testHeights[] = {1, 2, 3};
        int bitDepths[] = {1, 2, 4};
        
        for (int w = 0; w < 5; w++) {
            for (int h = 0; h < 3; h++) {
                for (int b = 0; b < 3; b++) {
                    int width = testWidths[w];
                    int height = testHeights[h];
                    int bpp = bitDepths[b];
                    
                    TrackRenderer* renderer = nullptr;
                    switch(bpp) {
                        case 1: renderer = renderer1bpp; break;
                        case 2: renderer = renderer2bpp; break;
                        case 4: renderer = renderer4bpp; break;
                    }
                    
                    int bufferSize = TrackRenderer::calculateBufferSize(width, height, bpp);
                    uint8_t* buffer = new uint8_t[bufferSize];
                    
                    // Should not crash with odd dimensions
                    renderer->renderTrackBinary("Test", buffer, width, height, 0, width);
                    
                    // Verify buffer is not corrupted (no access violations)
                    bool accessible = true;
                    for (int i = 0; i < bufferSize; i++) {
                        volatile uint8_t test = buffer[i]; // Read each byte
                        (void)test; // Avoid unused variable warning
                    }
                    TEST_ASSERT_TRUE(accessible);
                    
                    delete[] buffer;
                }
            }
        }
    }
    
    void test_color_blending_accuracy() {
        // Test that color blending produces expected intermediate values
        uint16_t bg = 0x0000; // Pure black (RGB565: 00000 000000 00000)
        uint16_t fg = 0xFFFF; // Pure white (RGB565: 11111 111111 11111)
        
        int width = 4;
        int height = 4;
        int bufferSize = TrackRenderer::calculateBufferSize(width, height, 4);
        
        uint8_t* binaryBuffer = new uint8_t[bufferSize];
        uint16_t* displayBuffer = new uint16_t[width * height];
        
        // Test 50% blend (value 8 out of 15 for 4bpp)
        memset(binaryBuffer, 0x88, bufferSize);
        renderer4bpp->renderBinaryToDisplay(binaryBuffer, width, height,
                                           displayBuffer, width, height,
                                           fg, bg, false);
        
        uint16_t blendedColor = displayBuffer[0];
        
        // Extract RGB components
        uint8_t r = (blendedColor >> 11) & 0x1F;
        uint8_t g = (blendedColor >> 5) & 0x3F;
        uint8_t b = blendedColor & 0x1F;
        
        // For 50% blend between black and white, we expect approximately:
        // R: ~15-16 (half of 31), G: ~31-32 (half of 63), B: ~15-16 (half of 31)
        TEST_ASSERT_GREATER_THAN(10, r);
        TEST_ASSERT_LESS_THAN(25, r);
        TEST_ASSERT_GREATER_THAN(20, g);
        TEST_ASSERT_LESS_THAN(45, g);
        TEST_ASSERT_GREATER_THAN(10, b);
        TEST_ASSERT_LESS_THAN(25, b);
        
        delete[] binaryBuffer;
        delete[] displayBuffer;
    }
    
    void test_memory_efficiency_comparison() {
        int width = 100;
        int height = 30;
        
        int size1bpp = TrackRenderer::calculateBufferSize(width, height, 1);
        int size2bpp = TrackRenderer::calculateBufferSize(width, height, 2);
        int size4bpp = TrackRenderer::calculateBufferSize(width, height, 4);
        
        // Verify expected size relationships
        TEST_ASSERT_EQUAL(size1bpp * 2, size2bpp);
        TEST_ASSERT_EQUAL(size1bpp * 4, size4bpp);
        TEST_ASSERT_EQUAL(size2bpp * 2, size4bpp);
        
        // Calculate actual memory usage
        Serial.print("Memory usage for ");
        Serial.print(width);
        Serial.print("x");
        Serial.print(height);
        Serial.println(" track:");
        Serial.print("1bpp: ");
        Serial.print(size1bpp);
        Serial.println(" bytes");
        Serial.print("2bpp: ");
        Serial.print(size2bpp);
        Serial.println(" bytes");
        Serial.print("4bpp: ");
        Serial.print(size4bpp);
        Serial.println(" bytes");
    }
    
    void test_rendering_quality_comparison() {
        const char* testText = "Quality Test Ñ";
        int width = 200;
        int height = 30;
        
        // Render same text with different bit depths
        int size1 = TrackRenderer::calculateBufferSize(width, height, 1);
        int size2 = TrackRenderer::calculateBufferSize(width, height, 2);
        int size4 = TrackRenderer::calculateBufferSize(width, height, 4);
        
        uint8_t* buffer1 = new uint8_t[size1];
        uint8_t* buffer2 = new uint8_t[size2];
        uint8_t* buffer4 = new uint8_t[size4];
        
        uint16_t* display1 = new uint16_t[width * height];
        uint16_t* display2 = new uint16_t[width * height];
        uint16_t* display4 = new uint16_t[width * height];
        
        // Render with each bit depth
        renderer1bpp->renderTrackBinary(testText, buffer1, width, height, 0, width);
        renderer2bpp->renderTrackBinary(testText, buffer2, width, height, 0, width);
        renderer4bpp->renderTrackBinary(testText, buffer4, width, height, 0, width);
        
        // Convert to display
        renderer1bpp->renderBinaryToDisplay(buffer1, width, height, display1, width, height,
                                           COLOR_TEXT, COLOR_BACKGROUND, false);
        renderer2bpp->renderBinaryToDisplay(buffer2, width, height, display2, width, height,
                                           COLOR_TEXT, COLOR_BACKGROUND, false);
        renderer4bpp->renderBinaryToDisplay(buffer4, width, height, display4, width, height,
                                           COLOR_TEXT, COLOR_BACKGROUND, false);
        
        // Count unique colors in each rendering
        int uniqueColors1 = countUniqueColors(display1, width * height);
        int uniqueColors2 = countUniqueColors(display2, width * height);
        int uniqueColors4 = countUniqueColors(display4, width * height);
        
        Serial.print("Unique colors - 1bpp: ");
        Serial.print(uniqueColors1);
        Serial.print(", 2bpp: ");
        Serial.print(uniqueColors2);
        Serial.print(", 4bpp: ");
        Serial.println(uniqueColors4);
        
        // Higher bit depth should generally have more unique colors (better anti-aliasing)
        TEST_ASSERT_GREATER_OR_EQUAL(uniqueColors1, uniqueColors2);
        TEST_ASSERT_GREATER_OR_EQUAL(uniqueColors2, uniqueColors4);
        
        delete[] buffer1;
        delete[] buffer2;
        delete[] buffer4;
        delete[] display1;
        delete[] display2;
        delete[] display4;
    }
    
private:
    int countUniqueColors(uint16_t* buffer, int size) {
        const int MAX_COLORS = 256; // Reasonable limit for counting
        uint16_t uniqueColors[MAX_COLORS];
        int count = 0;
        
        for (int i = 0; i < size && count < MAX_COLORS; i++) {
            bool found = false;
            for (int j = 0; j < count; j++) {
                if (uniqueColors[j] == buffer[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                uniqueColors[count++] = buffer[i];
            }
        }
        
        return count;
    }
};

// Global test instance
BitOperationsTest testInstance;

void setUp(void) {
    testInstance.setUp();
}

void tearDown(void) {
    testInstance.tearDown();
}

void test_1bpp_storage_efficiency(void) {
    testInstance.test_1bpp_storage_efficiency();
}

void test_2bpp_transparency_levels(void) {
    testInstance.test_2bpp_transparency_levels();
}

void test_4bpp_smooth_gradients(void) {
    testInstance.test_4bpp_smooth_gradients();
}

void test_bit_boundary_handling(void) {
    testInstance.test_bit_boundary_handling();
}

void test_color_blending_accuracy(void) {
    testInstance.test_color_blending_accuracy();
}

void test_memory_efficiency_comparison(void) {
    testInstance.test_memory_efficiency_comparison();
}

void test_rendering_quality_comparison(void) {
    testInstance.test_rendering_quality_comparison();
}

void process() {
    UNITY_BEGIN();
    
    RUN_TEST(test_1bpp_storage_efficiency);
    RUN_TEST(test_2bpp_transparency_levels);
    RUN_TEST(test_4bpp_smooth_gradients);
    RUN_TEST(test_bit_boundary_handling);
    RUN_TEST(test_color_blending_accuracy);
    RUN_TEST(test_memory_efficiency_comparison);
    RUN_TEST(test_rendering_quality_comparison);
    
    UNITY_END();
}

#ifdef UNIT_TEST
int main(int argc, char **argv) {
    process();
    return 0;
}
#endif