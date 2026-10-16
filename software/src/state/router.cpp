#include "router.h"

Router::Router() : stackDepth(0) {
  name[0] = '\0';
  // routeStack[stackDepth] (index 0 while nothing has been pushed yet) is
  // read by getCurrentRoute() from the very start, so it must already be a
  // valid ROOT route rather than indeterminate memory. This happened to work
  // for the single global `state` instance only because static storage is
  // zero-initialized before any constructor runs (RouteType ROOT == 0) -
  // any locally-constructed Router/State (e.g. in tests) had no such luck.
  routeStack[0].type = Route_t::ROOT;
  routeStack[0].entityId = 0;
  routeStack[0].scope = Route_t::ROOT;
  routeStack[0].selectedIndex = 0;
  routeStack[0].topVisibleIndex = 0;
}

void Router::pushRoute(Route_t::RouteType type, uint32_t entityId,
                       const char *entityName) {
  pushRoute(type, entityId, entityName, routeStack[stackDepth].type);
}

void Router::pushRoute(Route_t::RouteType type, uint32_t entityId,
                       const char *entityName, Route_t::RouteType scope) {
  if (stackDepth >= MAX_ROUTE_DEPTH - 1)
    return;
  stackDepth++;
  routeStack[stackDepth].type = type;
  routeStack[stackDepth].entityId = entityId;
  routeStack[stackDepth].scope = scope;

  // Initialize selection state for new route
  routeStack[stackDepth].selectedIndex = 0;
  routeStack[stackDepth].topVisibleIndex = 0;

  setName(entityName);
}

bool Router::popRoute() {
  if (stackDepth > 0) {
    stackDepth--;
    name[0] = '\0';
    return true;
  }
  return false;
}

void Router::setName(const char *n) {
  if (n) {
    strncpy(name, n, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
  } else {
    name[0] = '\0';
  }
}

Route_t &Router::getCurrentRoute() { return routeStack[stackDepth]; }

bool Router::canGoBack() { return stackDepth > 0; }

int Router::getDepth() { return stackDepth; }

void Router::saveCurrentSelection(int selectedIndex, int topVisibleIndex) {
  routeStack[stackDepth].selectedIndex = selectedIndex;
  routeStack[stackDepth].topVisibleIndex = topVisibleIndex;
}