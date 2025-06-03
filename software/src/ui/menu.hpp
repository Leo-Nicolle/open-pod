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
    
    // Render a single row of a menu item
    void renderRow(int number, const char* title, bool selected, int rowInItem) {
        uint16_t* rowBuffer = g_buffers.getRowBuffer();
        
        uint16_t bgColor = selected ? COLOR_ACCENT : COLOR_BACKGROUND;
        uint16_t textColor = selected ? COLOR_BACKGROUND : COLOR_TEXT;
        uint16_t numColor = 0x4208; // Dimmed color for track numbers
        
        // Clear row with background color (excluding scrollbar area)
        for (int x = 0; x < SCROLLBAR_X; x++) {
            rowBuffer[x] = bgColor;
        }
        
        // Only render text in the visible text area (rows 8-24 approximately)
        if (rowInItem >= 8 && rowInItem < 24 && title) {
            int textRow = rowInItem - 8;
            
            // Create temporary renderer for this scanline
            FastFontRenderer tempRenderer(rowBuffer, SCROLLBAR_X, 1);
            
            // Render track number (safely)
            if (number > 0 && number < 1000) { // Sanity check
                char numberStr[8];
                snprintf(numberStr, sizeof(numberStr), "%d", number);
                tempRenderer.renderTextScanline(numberStr, 5, textRow, smallFont, 
                                               numColor, bgColor, rowBuffer);
            }
            
            // Render title with truncation if needed
            if (strlen(title) > 0) {
                int titleStartX = 30;
                int maxTitleWidth = SCROLLBAR_X - titleStartX - 10;
                
                // Check if title fits
                int titleWidth = tempRenderer.measureText(title, font);
                
                if (titleWidth <= maxTitleWidth) {
                    // Title fits, render directly
                    tempRenderer.renderTextScanline(title, titleStartX, textRow, font, 
                                                  textColor, bgColor, rowBuffer);
                } else {
                    // Title needs truncation - create truncated version
                    char truncated[64];
                    truncateTitle(title, truncated, sizeof(truncated), maxTitleWidth, tempRenderer);
                    tempRenderer.renderTextScanline(truncated, titleStartX, textRow, font, 
                                                  textColor, bgColor, rowBuffer);
                }
            }
        }
    }
    
    // Render a single column of a menu item
    void renderColumn(int number, const char* title, bool selected, int columnInItem) {
        uint16_t* columnBuffer = g_buffers.getColumnBuffer();
        
        uint16_t bgColor = selected ? COLOR_ACCENT : COLOR_BACKGROUND;
        uint16_t textColor = selected ? COLOR_BACKGROUND : COLOR_TEXT;
        uint16_t numColor = 0x4208;
        
        // Initialize column with background (only for track height)
        for (int y = 0; y < TRACK_HEIGHT; y++) {
            columnBuffer[y] = bgColor;
        }
        
        // Only render if we have valid data
        if (!title || number <= 0) return;
        
        // Create temporary renderer for column rendering
        FastFontRenderer tempRenderer(columnBuffer, 1, TRACK_HEIGHT);
        
        // Render track number area (columns 5-25)
        if (columnInItem >= 5 && columnInItem < 25) {
            char numberStr[8];
            snprintf(numberStr, sizeof(numberStr), "%d", number);
            tempRenderer.renderTextColumn(numberStr, 5, 8, smallFont, 
                                        numColor, bgColor, columnBuffer);
        }
        
        // Render title area (columns 30+)
        if (columnInItem >= 30 && columnInItem < SCROLLBAR_X - 10) {
            int maxTitleWidth = SCROLLBAR_X - 40;
            int titleWidth = tempRenderer.measureText(title, font);
            
            const char* textToRender = title;
            char truncated[64];
            
            if (titleWidth > maxTitleWidth) {
                truncateTitle(title, truncated, sizeof(truncated), maxTitleWidth, tempRenderer);
                textToRender = truncated;
            }
            
            tempRenderer.renderTextColumn(textToRender, 30, 8, font, 
                                        textColor, bgColor, columnBuffer);
        }
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