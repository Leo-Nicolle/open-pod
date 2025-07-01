#pragma once
#include <Arduino.h>

struct Route_t {
  enum RouteType { ROOT, SEARCH_RESULTS, GENRES, ARTISTS, ALBUMS, TRACKS };
  RouteType type;
  uint32_t entityId;     // ID of current entity (genre_id, artist_id, etc.)
  char entityName[64];   // Display name for breadcrumb
  uint32_t currentPage;  // For pagination
  uint32_t totalResults; // Total items available
  bool hasMore;          // Whether more pages exist
};
class State; // Forward declaration for friend access
#define MAX_ROUTE_DEPTH 8

class Router {
  friend class State; // Allow State to access private members
private:
  Route_t routeStack[MAX_ROUTE_DEPTH];
  int stackDepth;

public:
  Router();

  void pushRoute(Route_t::RouteType type, uint32_t entityId = 0,
                 const char *entityName = nullptr);
  bool popRoute();
  Route_t &getCurrentRoute();
  bool canGoBack();
  int getDepth();
};
