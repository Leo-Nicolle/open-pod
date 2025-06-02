#pragma once
#include <Arduino.h>
#include "fonts/Fastfont.h"
#include "ui/theme.h"
// Color cache for fast blending
class ColorCache {
private:
  struct CacheEntry {
    uint16_t bgColor;
    uint16_t fgColor;
    uint16_t blendTable[256]; // Pre-computed blend values
    bool valid;
  };
  
  static const int CACHE_SIZE = 4;
  CacheEntry cache[CACHE_SIZE];
  int nextSlot = 0;
  
public:
  // Get pre-computed blend table for color pair
  const uint16_t* getBlendTable(uint16_t bgColor, uint16_t fgColor) {
    // Check cache
    for (int i = 0; i < CACHE_SIZE; i++) {
      if (cache[i].valid && 
          cache[i].bgColor == bgColor && 
          cache[i].fgColor == fgColor) {
        return cache[i].blendTable;
      }
    }
    
    // Not in cache, compute it
    CacheEntry& entry = cache[nextSlot];
    entry.bgColor = bgColor;
    entry.fgColor = fgColor;
    entry.valid = true;
    
    // Pre-compute all 256 blend levels
    computeBlendTable(bgColor, fgColor, entry.blendTable);
    
    nextSlot = (nextSlot + 1) % CACHE_SIZE;
    return entry.blendTable;
  }
  
private:
  void computeBlendTable(uint16_t bg, uint16_t fg, uint16_t* table) {
    // Extract RGB components
    uint8_t bg_r = (bg >> 11) & 0x1F;
    uint8_t bg_g = (bg >> 5) & 0x3F;
    uint8_t bg_b = bg & 0x1F;
    
    uint8_t fg_r = (fg >> 11) & 0x1F;
    uint8_t fg_g = (fg >> 5) & 0x3F;
    uint8_t fg_b = fg & 0x1F;
    
    // Compute blend for each alpha level
    for (int alpha = 0; alpha < 256; alpha++) {
      uint8_t r = bg_r + ((fg_r - bg_r) * alpha) / 255;
      uint8_t g = bg_g + ((fg_g - bg_g) * alpha) / 255;
      uint8_t b = bg_b + ((fg_b - bg_b) * alpha) / 255;
      
      table[alpha] = (r << 11) | (g << 5) | b;
    }
  }
};

// Main font renderer
class FastFontRenderer {
private:
  ColorCache colorCache;
  uint16_t* renderBuffer;
  int bufferWidth;
  int bufferHeight;
  
public:
  FastFontRenderer(uint16_t* buffer, int width, int height) 
    : renderBuffer(buffer), bufferWidth(width), bufferHeight(height) {}
  
  // Render text to buffer with anti-aliasing
  int renderText(const char* text, int x, int y, 
                 const FastFont& font, uint16_t color, uint16_t bgColor) {
    int startX = x;
    const uint16_t* blendTable = colorCache.getBlendTable(bgColor, color);
    
    while (*text) {
      char c = *text++;
      
      // Skip characters outside font range
      if (c < font.firstChar || c > font.lastChar) {
        continue;
      }
      
      const FastGlyph& glyph = font.glyphs[c - font.firstChar];
      
      // Apply kerning if available
      if (font.hasKerning && *text) {
        x += getKerning(font, c, *text);
      }
      
      // Render glyph
      renderGlyph(glyph, x + glyph.xOffset, y + glyph.yOffset, blendTable, bgColor);
      
      x += glyph.xAdvance;
    }
    
    return x - startX; // Return rendered width
  }
  
  // Render single glyph with alpha blending
  void renderGlyph(const FastGlyph& glyph, int x, int y, 
                   const uint16_t* blendTable, uint16_t bgColor) {
    const uint8_t* alphaPtr = glyph.alphaData;
    
    for (int dy = 0; dy < glyph.height; dy++) {
      if (y + dy < 0 || y + dy >= bufferHeight) continue;
      
      int bufferOffset = (y + dy) * bufferWidth + x;
      
      for (int dx = 0; dx < glyph.width; dx++) {
        if (x + dx < 0 || x + dx >= bufferWidth) continue;
        
        uint8_t alpha = *alphaPtr++;
        
        if (alpha == 0) {
          // Fully transparent
          renderBuffer[bufferOffset + dx] = bgColor;
        } else if (alpha == 255) {
          // Fully opaque
          renderBuffer[bufferOffset + dx] = blendTable[255];
        } else {
          // Blend using pre-computed table
          renderBuffer[bufferOffset + dx] = blendTable[alpha];
        }
      }
    }
  }
  
  // Measure text width without rendering
  int measureText(const char* text, const FastFont& font) {
    int width = 0;
    
    while (*text) {
      char c = *text++;
      
      if (c < font.firstChar || c > font.lastChar) {
        continue;
      }
      
      const FastGlyph& glyph = font.glyphs[c - font.firstChar];
      width += glyph.xAdvance;
      
      // Apply kerning
      if (font.hasKerning && *text) {
        width += getKerning(font, c, *text);
      }
    }
    
    return width;
  }
  
  // Render with alignment
  void renderTextAligned(const char* text, int x, int y, int maxWidth,
                        const FastFont& font, uint16_t color, uint16_t bgColor,
                        int align = 0) { // 0=left, 1=center, 2=right
    if (align == 0) {
      renderText(text, x, y, font, color, bgColor);
      return;
    }
    
    int textWidth = measureText(text, font);
    
    if (align == 1) { // Center
      x += (maxWidth - textWidth) / 2;
    } else if (align == 2) { // Right
      x += maxWidth - textWidth;
    }
    
    renderText(text, x, y, font, color, bgColor);
  }
  
private:
  int8_t getKerning(const FastFont& font, char c1, char c2) {
    if (!font.hasKerning || !font.kerningTable) return 0;
    
    // Simple kerning table lookup
    // You'd implement the actual lookup based on your kerning format
    return 0;
  }
};

// Font data would be generated by a conversion tool
// Example structure for IBM Plex Sans 16pt
extern const FastFont IBMPlexSans16;
extern const FastFont IBMPlexSans16Bold;
extern const FastFont IBMPlexSans12;

// Python script to generate font data:
/*
import freetype
import numpy as np

def generate_font_data(font_path, size, output_name):
    face = freetype.Face(font_path)
    face.set_pixel_sizes(0, size)
    
    glyphs = []
    for char_code in range(32, 127):  # ASCII printable
        face.load_char(chr(char_code), freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL)
        glyph = face.glyph
        bitmap = glyph.bitmap
        
        # Extract alpha channel
        alpha_data = np.array(bitmap.buffer).reshape(bitmap.rows, bitmap.width)
        
        # Generate C array
        glyph_data = {
            'width': bitmap.width,
            'height': bitmap.rows,
            'xOffset': glyph.bitmap_left,
            'yOffset': -glyph.bitmap_top,
            'xAdvance': glyph.advance.x >> 6,
            'alphaData': alpha_data.flatten().tolist()
        }
        glyphs.append(glyph_data)
    
    # Generate C++ code
    generate_cpp_code(glyphs, output_name)
*/

// Ultra-fast specialized renderer for menu items
class MenuItemRenderer {
private:
  uint16_t* buffer;
  const FastFont& font;
  const FastFont& smallFont;
  FastFontRenderer renderer;
  
public:
  MenuItemRenderer(uint16_t* buf, const FastFont& mainFont, const FastFont& numFont)
    : buffer(buf), font(mainFont), smallFont(numFont), renderer(buf, 320, 30) {}
  
void renderMenuItem(int number, const char* title, bool selected, bool previousSelected) {
  uint16_t bgColor = selected ? COLOR_ACCENT : COLOR_BACKGROUND;
  uint16_t textColor = selected ? COLOR_BACKGROUND : COLOR_TEXT;
  uint16_t numColor = 0x4208;

  // Clear background (optimized, excludes scrollbar area)
  const int menuWidth = SCROLLBAR_X;  // Leave scrollbar at x = 312
  uint32_t BG = selected ? COLOR_ACCENT_32: COLOR_BACKGROUND_32;
  uint32_t* buf32 = (uint32_t*)buffer;
  for(int i = 0; i < (30 * 320)/2; i++) {
      *buf32++ = BG;
  }

  // Render title (with truncation if needed)
  int maxTitleWidth = 280; // Leave room for scrollbar
  int titleWidth = renderer.measureText(title, font);

  if (titleWidth <= maxTitleWidth) {
    renderer.renderText(title, 5, 15, font, textColor, bgColor);
  } else {
    // Truncate with ellipsis
    char truncated[32];
    int i = 0;
    int width = 0;
    int ellipsisWidth = renderer.measureText("...", font);

    while (title[i] && width + ellipsisWidth < maxTitleWidth) {
      truncated[i] = title[i];
      width = renderer.measureText(truncated, font);
      i++;
    }

    strcpy(&truncated[i - 1], "...");
    renderer.renderText(truncated, 30, 15, font, textColor, bgColor);
  }
}

};