#include "static_menu.h"

// Static menu item arrays for efficient access
static const char* rootMenuLabels[] = {
  "Tracks",
  "Artists", 
  "Albums",
  "Genres",
  "Search",
  "Settings",
  "Now Playing"
};

static const char* optionsMenuLabels[] = {
  "Shuffle",
  "Repeat",
  "Equalizer",
  "Sleep Timer",
  "About"
};

static const char* settingsMenuLabels[] = {
  "Display",
  "Audio",
  "System",
  "Reset",
  "Back"
};

// Global menu element arrays for compatibility
const char* rootMenuElements[] = {
  "Tracks",
  "Artists", 
  "Albums",
  "Genres",
  "Search",
  "Settings",
  "Now Playing"
};

const char* optionsMenuElements[] = {
  "Shuffle",
  "Repeat",
  "Equalizer",
  "Sleep Timer",
  "About"
};

const uint32_t rootMenuCount = sizeof(rootMenuElements) / sizeof(rootMenuElements[0]);
const uint32_t optionsMenuCount = sizeof(optionsMenuElements) / sizeof(optionsMenuElements[0]);

// StaticMenu implementation
StaticMenu::StaticMenu(MenuType menuType) : itemCount(0), type(menuType) {
  // Initialize with empty items
  for (int i = 0; i < MAX_MENU_ITEMS; i++) {
    items[i] = {nullptr, 0, false};
  }
}

void StaticMenu::addItem(const char* label, uint32_t id, bool enabled) {
  if (itemCount < MAX_MENU_ITEMS) {
    items[itemCount] = {label, id, enabled};
    itemCount++;
  }
}

void StaticMenu::clearItems() {
  itemCount = 0;
  for (int i = 0; i < MAX_MENU_ITEMS; i++) {
    items[i] = {nullptr, 0, false};
  }
}

const MenuItem& StaticMenu::getItem(uint32_t index) const {
  static MenuItem emptyItem = {nullptr, 0, false};
  if (index < itemCount) {
    return items[index];
  }
  return emptyItem;
}

const char** StaticMenu::getItemLabels() const {
  static const char* labels[MAX_MENU_ITEMS];
  for (uint32_t i = 0; i < itemCount; i++) {
    labels[i] = items[i].label;
  }
  return labels;
}

// Static factory methods
StaticMenu StaticMenu::createRootMenu() {
  StaticMenu menu(MENU_ROOT);
  menu.addItem("Tracks", 1, true);
  menu.addItem("Artists", 2, true);
  menu.addItem("Albums", 3, true);
  menu.addItem("Genres", 4, true);
  menu.addItem("Search", 5, true);
  menu.addItem("Settings", 6, true);
  menu.addItem("Now Playing", 7, true);
  return menu;
}

StaticMenu StaticMenu::createOptionsMenu() {
  StaticMenu menu(MENU_OPTIONS);
  menu.addItem("Shuffle", 10, true);
  menu.addItem("Repeat", 11, true);
  menu.addItem("Equalizer", 12, true);
  menu.addItem("Sleep Timer", 13, true);
  menu.addItem("About", 14, true);
  return menu;
}

StaticMenu StaticMenu::createSettingsMenu() {
  StaticMenu menu(MENU_SETTINGS);
  menu.addItem("Display", 20, true);
  menu.addItem("Audio", 21, true);
  menu.addItem("System", 22, true);
  menu.addItem("Reset", 23, true);
  menu.addItem("Back", 24, true);
  return menu;
}

// Global menu instances
StaticMenu rootMenu = StaticMenu::createRootMenu();
StaticMenu optionsMenu = StaticMenu::createOptionsMenu();
StaticMenu settingsMenu = StaticMenu::createSettingsMenu();