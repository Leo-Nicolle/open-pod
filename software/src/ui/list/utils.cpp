#include "utils.h"

int calculateBufferSize(int width, int height, int bpp) {
  int totalBits = width * height * bpp;
  return (totalBits + 7) / 8;
}