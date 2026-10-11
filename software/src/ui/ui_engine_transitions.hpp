#pragma once
// OpenPodUIEngine route-transition slide animations.
// Included at the end of ui_engine.hpp - include that instead (it may only
// be included from one translation unit, see app/app.h).
#include "ui_engine.hpp"

void OpenPodUIEngine::transitionToNowPlaying()
{
  // Set transitioning state
  lastDrawnOffset = 0;
  state.setAnimating(true); // Set animating state

  // Blank the header and scrollbar before the scroll-slide starts, instead
  // of trying to redraw them in place every frame while setScrollOffset()
  // is active. Every GRAM write during the transition races the panel's own
  // free-running refresh (no Tearing Effect line is wired up on this
  // board), and repeatedly re-pushing the same small region each frame to
  // chase the moving scroll offset made that race visible as a persistent
  // shimmer. A one-time blank here has nothing to race - it happens before
  // the scroll offset ever moves - and renderCurrentState() draws the real
  // header (and, for a track-list screen, scrollbar) back once the
  // transition completes and the scroll offset is back at rest.
  display->fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_SURFACE);
  // Always blank the scrollbar column, even when neither menu shows a
  // scrollbar: with the hardware scroll it is the seam between the old and
  // new screen that travels across the display, so it must be plain padding.
  display->fillRect(SCROLLBAR_X, BODY_Y, SCREEN_WIDTH - SCROLLBAR_X,
                    BODY_HEIGHT, COLOR_BG);

  uint32_t animId = animManager.animate(
      ANIM_CUSTOM, 0, SCREEN_WIDTH, 800,
      [this](float offset)
      {
        int currentOffset = (int)offset;
        int width = currentOffset - lastDrawnOffset;
        // Guard against a non-positive width (e.g. a transient glitch in the
        // eased offset) rather than handing it to the renderer/display -
        // a negative width there can wrap to a huge pixel count and hang.
        if (width <= 0)
          return;
        for (int y = BODY_Y; y < SCREEN_HEIGHT; y += CHUNK_HEIGHT)
        {
          nowPlaying.renderChunk(display, lastDrawnOffset, y, width);
        }
        int scrollOffset = SCREEN_WIDTH - currentOffset;
        display->setScrollOffset(scrollOffset);
        lastDrawnOffset = currentOffset;
      },
      [this]()
      {
        state.setAnimating(false);
      },
      Easing::easeInOutCubic);
}

void OpenPodUIEngine::transitionToTrackList(bool leftToRight)
{
  lastDrawnOffset = 0;
  animatingLeftToRight = leftToRight;
  state.setAnimating(true);

  // Temporary: reset per-frame timing stats so completeFunc's
  // measurePerformance() call reports numbers for just THIS transition, to
  // find out empirically whether leftToRight=true (back navigation) really
  // renders slower per frame than leftToRight=false, or whether it's a
  // one-time hitch (e.g. State::back()'s extra restoreSelectionState() call)
  // before the animation starts. Remove once that's answered.
  animManager.resetPerformanceStats();

  // Blank the header and scrollbar before the scroll-slide starts - see the
  // matching comment in transitionToNowPlaying() for why (no Tearing Effect
  // line on this board, so repeatedly re-pushing them each frame to chase
  // the moving scroll offset raced the panel's own refresh and shimmered).
  // renderCurrentState() draws the real ones back once this completes.
  display->fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_SURFACE);
  // Always blank the scrollbar column, even when neither menu shows a
  // scrollbar: with the hardware scroll it is the seam between the old and
  // new screen that travels across the display, so it must be plain padding.
  display->fillRect(SCROLLBAR_X, BODY_Y, SCREEN_WIDTH - SCROLLBAR_X,
                    BODY_HEIGHT, COLOR_BG);

  uint32_t animId = animManager.animate(
      ANIM_CUSTOM, 0, SCREEN_WIDTH, 800,
      [this](float offset)
      {
        int currentOffset = (int)offset;
        int width = currentOffset - lastDrawnOffset;
        // Guard against a non-positive width (e.g. a transient glitch in the
        // eased offset) rather than handing it to the renderer/display -
        // a negative width there can wrap to a huge pixel count and hang.
        if (width <= 0)
          return;

        // Fixed coordinate calculations
        int x = animatingLeftToRight ? SCREEN_WIDTH - currentOffset
                                     : lastDrawnOffset;
        // g_buffers' scratch buffers are sized for one CHUNK_HEIGHT-tall
        // strip (SCREEN_WIDTH * CHUNK_HEIGHT pixels), same as every other
        // renderer in this codebase respects. Rendering the full BODY_HEIGHT
        // in one renderRect call (as this used to) overflows that buffer
        // whenever a frame's width grows past ~46px - which normally only
        // takes ~7px/frame, but does happen when a slow tick (e.g. the audio
        // pipeline's PSRAM refill while a track is playing) delays ui.update()
        // and several frames' worth of movement land in a single call.
        for (int chunkY = 0; chunkY < BODY_HEIGHT; chunkY += CHUNK_HEIGHT)
        {
          int chunkHeight = min(CHUNK_HEIGHT, BODY_HEIGHT - chunkY);
          renderBodySlice(x, chunkY, width, chunkHeight);
        }

        // Fixed scroll offset
        int scrollOffset = animatingLeftToRight ? currentOffset
                                                : SCREEN_WIDTH - currentOffset;
        display->setScrollOffset(scrollOffset);
        lastDrawnOffset = currentOffset;
      },
      [this, leftToRight]()
      {
        state.setAnimating(false);
        if (LOG_ENABLED(UI))
        {
          measurePerformance();
        }
      },
      Easing::easeInOutCubic);
}
