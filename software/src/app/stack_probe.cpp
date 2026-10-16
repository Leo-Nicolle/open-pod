#include "stack_probe.h"
#include <Arduino.h>

extern "C" char *sbrk(int incr);
extern "C" char _estack; // top of RAM, from the linker script

static const uint32_t PATTERN = 0xA5A5A5A5u;
static uint32_t *paintEnd = nullptr; // one past the highest painted word

static uint32_t *heapEnd() {
  return (uint32_t *)(((uintptr_t)sbrk(0) + 3) & ~(uintptr_t)3);
}

__attribute__((noinline)) void stackPaint() {
  // 64 B under the stack pointer: this frame and the calls it makes.
  uint32_t *top = (uint32_t *)((__get_MSP() - 64) & ~(uintptr_t)3);
  for (uint32_t *p = heapEnd(); p < top; p++) {
    *p = PATTERN;
  }
  paintEnd = top;
}

// The heap may have grown into the painted region since: scanning from the
// current heap end leaves its blocks out.
static uint32_t *deepestStack() {
  uint32_t *p = heapEnd();
  while (p < paintEnd && *p == PATTERN) {
    p++;
  }
  return p;
}

uint32_t stackFreeMin() {
  if (!paintEnd) {
    return 0;
  }
  return (uint32_t)((char *)deepestStack() - (char *)heapEnd());
}

uint32_t stackUsedMax() {
  if (!paintEnd) {
    return 0;
  }
  return (uint32_t)(&_estack - (char *)deepestStack());
}
