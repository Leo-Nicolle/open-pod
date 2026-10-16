#pragma once
// OpenPodUIEngine state-event dispatch and handlers.
// Included at the end of ui_engine.hpp - include that instead (it may only
// be included from one translation unit, see app/app.h).
#include "ui_engine.hpp"

// Static event handler - routes events to instance methods
void OpenPodUIEngine::onStateEvent(int eventType, void *eventData,
                                   EventTarget *source)
{
  if (!instance)
  {
    LOG_WARN("UI Engine instance not available for event handling");
    return;
  }

  switch (eventType)
  {
  case EVENT_TRACK_SELECTED:
    instance->handleTrackSelected((TrackSelectedEvent *)eventData);
    break;

  case EVENT_SCROLL_CHANGED:
    instance->handleScrollChanged((ScrollChangedEvent *)eventData);
    break;

  case EVENT_PAGE_CHANGED:
    instance->handlePageChanged((ScrollChangedEvent *)eventData);
    break;

  case EVENT_PLAYBACK_STARTED:
    instance->handlePlaybackStarted((PlaybackEvent *)eventData);
    break;

  case EVENT_PLAYBACK_PAUSED:
    instance->handlePlaybackPaused((PlaybackEvent *)eventData);
    break;

  case EVENT_PLAYBACK_RESUMED:
    instance->handlePlaybackResumed((PlaybackEvent *)eventData);
    break;

  case EVENT_PLAYBACK_STOPPED:
    instance->handlePlaybackStopped((PlaybackEvent *)eventData);
    break;

  case EVENT_PROGRESS_UPDATED:
    instance->handleProgressUpdated((ProgressEvent *)eventData);
    break;

  case EVENT_TRACK_ENDED:
    instance->handleTrackEnded((PlaybackEvent *)eventData);
    break;

  case EVENT_ROUTE_CHANGED:
    instance->handleNavigationChanged((RouteChangedEvent *)eventData);
    break;

  case EVENT_ANIMATION_STARTED:
    instance->handleAnimationStarted((AnimationEvent *)eventData);
    break;

  case EVENT_ANIMATION_FINISHED:
    instance->handleAnimationFinished((AnimationEvent *)eventData);
    break;

  case EVENT_TRACK_LIST_UPDATED:
    instance->handleTrackListUpdated((ListEvent *)eventData);
    break;

  case EVENT_TRACK_DURATION_CHANGED:
    instance->handleTrackDurationChanged((int *)eventData);
    break;

  case EVENT_VOLUME_CHANGED:
    instance->handleVolumeChanged((VolumeEvent *)eventData);
    break;

  case EVENT_SEEK_MODE_CHANGED:
    instance->handleSeekModeChanged((SeekModeEvent *)eventData);
    break;

  case EVENT_SEARCH_CHANGED:
    instance->handleSearchChanged((SearchEvent *)eventData);
    break;

  case EVENT_FAST_SCROLL_CHANGED:
    instance->handleFastScrollChanged((FastScrollEvent *)eventData);
    break;

  default:
    LOG_CAT(UI, "UI Engine: Unhandled event type: %d", eventType);
    break;
  }
}

void OpenPodUIEngine::handleTrackSelected(const TrackSelectedEvent *event)
{
  LOG_CAT(UI, "Track selected - %s", event->trackName);

  // Update UI components
  updateTrackListScroll();

  // Re-render if we're in track list view
  if (state.getCurrentRoute().type != Route_t::RouteType::NOW_PLAYING)
  {
    renderTrackListArea();
  }
}

void OpenPodUIEngine::handleScrollChanged(const ScrollChangedEvent *event)
{
  LOG_CAT(UI, "Scroll changed - selected: %d, top visible: %d",
          event->selectedIndex, event->topVisibleIndex);
  elementList.setScroll(event->selectedIndex, event->topVisibleIndex);

  updateScrollbar();
  updateFastScrollOverlay();

  // Handle cache updates for track list
  if (event->topVisibleIndex != event->oldTopVisibleIndex)
  {
    int scrollDelta = event->topVisibleIndex - event->oldTopVisibleIndex;
    if (scrollDelta > 0)
    {
      elementList.scrollDown(scrollDelta, event->visibleElements);
    }
    else
    {
      elementList.scrollUp(-scrollDelta, event->visibleElements);
    }
  }

  renderTrackListArea();
}

void OpenPodUIEngine::handlePageChanged(const ScrollChangedEvent *event)
{
  LOG_CAT(UI, "Page changed");
  for (int i = 0; LOG_ENABLED(UI) && i < ELEMENTS_PER_SCREEN; i++)
  {
    LOG_CAT(UI, "Track %d: %s", i, event->visibleElements[i]);
  }
  updateTrackListScroll();
  elementList.rebuildCache();
  updateScrollbar();
  updateFastScrollOverlay();
  renderTrackList(); // Full re-render for page changes
}

void OpenPodUIEngine::handlePlaybackStarted(const PlaybackEvent *event)
{
  LOG_CAT(UI, "Playback started - %s", event->trackName);

  // Update now playing component
  nowPlaying.setTrack(event->trackName);
  nowPlaying.setPlayState(true);
  updateAlbumArtForTrack(event->trackId);

  // If we're in now playing view, update display
  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }

  // Could also trigger transition to now playing view automatically
  // transitionToNowPlaying();
}

void OpenPodUIEngine::handlePlaybackPaused(const PlaybackEvent *event)
{
  LOG_CAT(UI, "Playback paused");

  nowPlaying.setPlayState(false);

  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handlePlaybackResumed(const PlaybackEvent *event)
{
  LOG_CAT(UI, "Playback resumed");

  nowPlaying.setPlayState(true);

  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handlePlaybackStopped(const PlaybackEvent *event)
{
  LOG_CAT(UI, "Playback stopped");

  nowPlaying.setPlayState(false);
  nowPlaying.setProgress(0);

  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handleProgressUpdated(const ProgressEvent *event)
{
  // Only update if we're showing the now playing screen
  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    nowPlaying.setProgress(event->position);
    nowPlaying.setTrackLength(event->duration);

    // Only redraw the progress bar + time text area, to avoid flicker
    nowPlaying.updateNowPlaying(display);
  }
}

void OpenPodUIEngine::handleTrackEnded(const PlaybackEvent *event)
{
  LOG_CAT(UI, "Track ended - %s", event->trackName);

  // Visual feedback for track end (could show brief message, etc.)
}

void OpenPodUIEngine::handleNavigationChanged(const RouteChangedEvent *event)
{
  LOG_CAT(UI, "State changed from %d to %d", (int)event->oldRouteType,
          (int)event->newRouteType);
  nowPlaying.invalidateArt(); // the transition draws over the whole screen
  updateFastScrollOverlay();  // route changes clear fast scroll
  if (onMenu())
  {
    // The slide draws the new screen's rows: have them ready.
    btMenu.sync(state.getCurrentRoute(), millis());
    menuView.invalidate();
  }
  if (onPodcast())
  {
    podcastMenu.sync(state.getCurrentRoute());
    podcastView.invalidate();
  }
  // return;
  // Handle transitions between states
  if (event->oldRouteType != Route_t::RouteType::NOW_PLAYING &&
      event->newRouteType == Route_t::RouteType::NOW_PLAYING)
  {
    transitionToNowPlaying();
  }
  else if (event->oldRouteType == Route_t::RouteType::NOW_PLAYING &&
           event->newRouteType != Route_t::RouteType::NOW_PLAYING)
  {
    transitionToTrackList(!event->isForward);
  }
  // Same type but animating: a podcast folder into/out of a subfolder. Only
  // the transition's end clears isAnimating, so it must run.
  else if (event->oldRouteType != event->newRouteType ||
           state.getIsAnimating())
  {
    transitionToTrackList(!event->isForward);
  }
  else
  {
    // Direct state change without transition
    renderCurrentState();
  }
}

void OpenPodUIEngine::handleAnimationStarted(const AnimationEvent *event)
{
  LOG_CAT(UI, "Animation started - ID: %d", (int)event->animationId);
}

void OpenPodUIEngine::handleAnimationFinished(const AnimationEvent *event)
{
  LOG_CAT(UI, "Animation finished - ID: %d", (int)event->animationId);

  // Still sliding: a navigation cut the transition short (State::
  // endTransition). Drop the remaining frames and put the scroll back at
  // rest; the render below draws the screen it was heading to in one go.
  if (animManager.isAnimationActive(transitionAnimId))
  {
    animManager.cancel(transitionAnimId);
    display->setScrollOffset(0);
  }

  // Ensure final render after animation
  renderCurrentState();
}

void OpenPodUIEngine::handleTrackListUpdated(const ListEvent *event)
{
  LOG_CAT(UI, "Track list updated - %d tracks", (int)event->totalElements);
  elementList.setScroll(event->selectedIndex, event->topVisibleIndex);
  elementList.setElements(event->visibleElements, event->totalElements);
  updateScrollbar();
  // Re-render if we're showing the track list
  if (!booting && !state.getIsAnimating() && !onSearch() && !onMenu() &&
      !onPodcast())
  {
    renderTrackList();
  }
}

void OpenPodUIEngine::handleTrackDurationChanged(int *duration)
{
  LOG_CAT(UI, "Track duration changed to %d seconds", *duration);

  nowPlaying.setTrackLength(*duration);

  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }
}
void OpenPodUIEngine::handleVolumeChanged(const VolumeEvent *event)
{
  nowPlaying.setVolumeOverlay(event->volume, true);
  volumeOverlayHideAt = millis() + VOLUME_OVERLAY_HIDE_DELAY;
  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    nowPlaying.renderVolumeOverlay(display);
  }
}

void OpenPodUIEngine::handleSeekModeChanged(const SeekModeEvent *event)
{
  nowPlaying.setSeekModeActive(event->active);
  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handleSearchChanged(const SearchEvent *event)
{
  if (state.getIsAnimating() || !onSearch())
    return; // the transition's final render draws it

  const SearchModel &search = state.getSearch();
  uint8_t dirty = event->dirty;
  if (dirty & SEARCH_DIRTY_LAYOUT)
  {
    searchView.renderAll(display, search);
    return;
  }
  if (dirty & SEARCH_DIRTY_HEADER)
    searchView.renderChunk(display, search, 0);
  if ((dirty & SEARCH_DIRTY_RIBBON) && search.getMode() == SearchModel::TYPING)
    searchView.renderChunk(display, search, 1);
  if (dirty & SEARCH_DIRTY_ROWS)
  {
    searchView.renderRows(display, search);
  }
  else if (dirty & SEARCH_DIRTY_SELECTION)
  {
    // first didn't move: only the two rows whose highlight changed
    searchView.renderHitRow(display, search, event->oldSelected);
    searchView.renderHitRow(display, search, search.getSelected());
  }
}

void OpenPodUIEngine::handleFastScrollChanged(const FastScrollEvent *event)
{
  updateFastScrollOverlay();
  Route_t::RouteType route = state.getCurrentRoute().type;
  if (state.getIsAnimating() || onSearch() || onPodcast() ||
      route == Route_t::RouteType::NOW_PLAYING)
    return;
  // The overlay covers rows 2-4 (chunks 3-5) only.
  elementList.renderRows(display, 2, 4);
  scrollbar.render(display);
}
