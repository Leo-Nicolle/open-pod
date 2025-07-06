#pragma once
#include <Arduino.h>

// Maximum number of menu items
#define MAX_MENU_ITEMS 10

// Menu item structure
struct MenuItem {
  const char* label;
  uint32_t id;
  bool enabled;
};

// Static menu types
enum MenuType {
  MENU_ROOT,
  MENU_OPTIONS,
  MENU_SETTINGS
};

class StaticMenu {
private:
  MenuItem items[MAX_MENU_ITEMS];
  uint32_t itemCount;
  MenuType type;
  
public:
  StaticMenu(MenuType menuType);
  
  // Menu management
  void addItem(const char* label, uint32_t id, bool enabled = true);
  void clearItems();
  
  // Getters
  uint32_t getItemCount() const { return itemCount; }
  const MenuItem& getItem(uint32_t index) const;
  const char** getItemLabels() const;
  MenuType getType() const { return type; }
  
  // Static menu definitions
  static StaticMenu createRootMenu();
  static StaticMenu createOptionsMenu();
  static StaticMenu createSettingsMenu();
};

// Global static menu instances
extern StaticMenu rootMenu;
extern StaticMenu optionsMenu;
extern StaticMenu settingsMenu;

// Root menu elements
extern const char* rootMenuElements[];
extern const uint32_t rootMenuCount;

// Options menu elements  
extern const char* optionsMenuElements[];
extern const uint32_t optionsMenuCount;