#include "../../src/ui/ui_types.h"

// Minimal dummy implementations for test
BufferManager g_buffers;
FastFontRenderer fontRenderer(g_buffers.getCurrentBuffer(), SCREEN_WIDTH, CHUNK_HEIGHT);