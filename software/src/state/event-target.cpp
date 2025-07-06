#include "event-target.h"

EventTarget::EventTarget() : listenerCount(0) {
  for (int i = 0; i < MAX_EVENT_LISTENERS; i++) {
    listeners[i] = nullptr;
  }
}

bool EventTarget::addEventListener(EventCallback callback) {
  if (listenerCount < MAX_EVENT_LISTENERS && callback != nullptr) {
    for (int i = 0; i < listenerCount; i++) {
      if (listeners[i] == callback) {
        return false;
      }
    }
    listeners[listenerCount++] = callback;
    return true;
  }
  return false;
}

bool EventTarget::removeEventListener(EventCallback callback) {
  for (int i = 0; i < listenerCount; i++) {
    if (listeners[i] == callback) {
      for (int j = i; j < listenerCount - 1; j++) {
        listeners[j] = listeners[j + 1];
      }
      listenerCount--;
      listeners[listenerCount] = nullptr;
      return true;
    }
  }
  return false;
}

void EventTarget::clearAllListeners() {
  for (int i = 0; i < MAX_EVENT_LISTENERS; i++) {
    listeners[i] = nullptr;
  }
  listenerCount = 0;
}

void EventTarget::emitEvent(int eventType, void* eventData) {
  for (int i = 0; i < listenerCount; i++) {
    if (listeners[i]) {
      listeners[i](eventType, eventData, this);
    }
  }
}