#pragma once
#include "ui_types.h"
#include "../theme.h"
#include "../rendering/font_renderer.h"
#include "../fonts/IBMPlexSans12.h"
#include "../fonts/IBMPlexSans16.h"
#include <Arduino.h>

// Simplified menu item renderer using global buffers
class MenuItemRenderer {
private:
    const FastFont& font;
    const FastFont& smallFont;
    
public:
    MenuItemRenderer(const FastFont& mainFont, const FastFont& numFont)
        : font(mainFont), smallFont(numFont) {
    }
    

private:
    // Safe title truncation helper
    void truncateTitle(const char* title, char* truncated, size_t bufferSize, 
                      int maxWidth, FastFontRenderer& renderer) {
        if (!title || !truncated || bufferSize < 4) {
            if (truncated && bufferSize > 0) truncated[0] = '\0';
            return;
        }
        
        const char* ellipsis = "...";
        int ellipsisWidth = renderer.measureText(ellipsis, font);
        
        size_t titleLen = strlen(title);
        size_t maxLen = min(titleLen, bufferSize - 4); // Leave room for ellipsis
        
        // Find the longest substring that fits
        size_t bestLen = 0;
        for (size_t len = 1; len <= maxLen; len++) {
            strncpy(truncated, title, len);
            truncated[len] = '\0';
            
            int testWidth = renderer.measureText(truncated, font);
            if (testWidth + ellipsisWidth <= maxWidth) {
                bestLen = len;
            } else {
                break;
            }
        }
        
        // Create final string
        if (bestLen > 0 && bestLen < titleLen) {
            strncpy(truncated, title, bestLen);
            truncated[bestLen] = '\0';
            strncat(truncated, ellipsis, bufferSize - bestLen - 1);
        } else if (bestLen == titleLen) {
            strncpy(truncated, title, bufferSize - 1);
            truncated[bufferSize - 1] = '\0';
        } else {
            strncpy(truncated, ellipsis, bufferSize - 1);
            truncated[bufferSize - 1] = '\0';
        }
    }
};