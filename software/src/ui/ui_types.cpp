#include "ui_types.h"

// Global buffer manager instance
BufferManager g_buffers;
FontRenderer fontRenderer = FontRenderer(g_buffers.getCurrentBuffer(), SCREEN_WIDTH, CHUNK_HEIGHT);