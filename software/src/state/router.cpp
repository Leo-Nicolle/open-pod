#include "router.h"

Router::Router() : stackDepth(0) {}

void Router::pushRoute(Route_t::RouteType type, uint32_t entityId,
                       const char *entityName) {
  if (stackDepth >= MAX_ROUTE_DEPTH - 1)
    return;
  stackDepth++;
  routeStack[stackDepth].type = type;
  routeStack[stackDepth].entityId = entityId;
  routeStack[stackDepth].currentPage = 0;
  routeStack[stackDepth].totalResults = 0;
  routeStack[stackDepth].hasMore = false;

  // Initialize selection state for new route
  routeStack[stackDepth].selectedIndex = 0;
  routeStack[stackDepth].topVisibleIndex = 0;

  if (entityName) {
    strncpy(routeStack[stackDepth].entityName, entityName, 63);
    routeStack[stackDepth].entityName[63] = '\0';
  } else {
    routeStack[stackDepth].entityName[0] = '\0';
  }
}

bool Router::popRoute() {
  if (stackDepth > 0) {
    stackDepth--;
    return true;
  }
  return false;
}

Route_t &Router::getCurrentRoute() { return routeStack[stackDepth]; }

bool Router::canGoBack() { return stackDepth > 0; }

int Router::getDepth() { return stackDepth; }

void Router::saveCurrentSelection(int selectedIndex, int topVisibleIndex) {
  routeStack[stackDepth].selectedIndex = selectedIndex;
  routeStack[stackDepth].topVisibleIndex = topVisibleIndex;
}