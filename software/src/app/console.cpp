#include "app.h"
#include "../logging.h"

// The debug serial is read here, one line at a time, and nowhere else: lines
// starting with "AT" go to the Bluetooth module, lines starting with "#" are
// test bench commands (app/bench.cpp). Anything else is dropped.
static char line[96];
static uint8_t lineLen = 0;

static void dispatch()
{
  if (lineLen >= 2 && (line[0] == 'A' || line[0] == 'a') &&
      (line[1] == 'T' || line[1] == 't'))
  {
    onConsoleAtLine(line);
  }
  else if (lineLen >= 1 && line[0] == '#')
  {
    benchCommand(line + 1);
  }
}

void pollConsole()
{
  while (Serial.available())
  {
    char c = Serial.read();
    if (c == '\r' || c == '\n')
    {
      line[lineLen] = 0;
      if (lineLen)
        dispatch();
      lineLen = 0;
    }
    else if (c == '#')
    {
      // A bench command starts here, whatever came before: opening the port
      // can leave stray bytes in the line with no newline after them.
      line[0] = c;
      lineLen = 1;
    }
    else if (c == '\b' || c == 0x7F)
    {
      if (lineLen)
        lineLen--;
    }
    else if (lineLen < sizeof(line) - 1)
    {
      line[lineLen++] = c;
    }
  }
}
