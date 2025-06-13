#pragma once
#include <Arduino.h>
#include <algorithm>

bool isColorDark(uint16_t color);
float computeGamma(float alpha, bool darkBackground);
uint16_t blendColor(uint16_t bgColor, uint16_t fgColor, float alpha);