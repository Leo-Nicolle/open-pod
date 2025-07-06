#pragma once
#include <Arduino.h>

// Forward declarations
class EventTarget;

// Event callback function type
typedef void (*EventCallback)(int eventType, void* eventData, EventTarget* source);

// Maximum number of listeners per EventTarget (adjust based on your needs)
#define MAX_EVENT_LISTENERS 4

// Base event target class - handles event registration and emission
class EventTarget {
private:
  EventCallback listeners[MAX_EVENT_LISTENERS];
  int listenerCount;

protected:
  // Protected method for derived classes to emit events
  void emitEvent(int eventType, void* eventData = nullptr);

public:
  EventTarget();
  virtual ~EventTarget() = default;

  // Event system
  bool addEventListener(EventCallback callback);
  bool removeEventListener(EventCallback callback);
  void clearAllListeners();
  int getListenerCount() const { return listenerCount; }
};