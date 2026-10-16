#pragma once
// Stack high-water mark. stackPaint() fills the free RAM between the heap
// and the stack with a pattern at boot; the deepest the stack has reached is
// where the pattern stops, scanning up from the heap end. See
// notes/performance-and-memory.md ("Step 0") and notes/RAM.json.
#include <stdint.h>

// Call first thing in setup(): paints from the heap end up to just below the
// current stack pointer.
void stackPaint();
// Untouched bytes between the heap end and the deepest stack so far.
uint32_t stackFreeMin();
// Deepest stack so far, in bytes from the top of RAM (_estack).
uint32_t stackUsedMax();
