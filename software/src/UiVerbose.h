#pragma once
// Serial chatter about UI and input events (scroll positions, list pages,
// animations, wheel touches), off by default so the audio monitor lines stay
// readable. Build with -DOPENPOD_UI_VERBOSE=1 to get it back.
#ifndef OPENPOD_UI_VERBOSE
#define OPENPOD_UI_VERBOSE 0
#endif
static const bool kUiVerbose = OPENPOD_UI_VERBOSE;
