/**
 * Example usage of the updated MusicIndex with path lookup functionality
 * This shows how to search for tracks and get their file paths
 */

#include "storage/Music_index.h"
#include "storage/PSRAM_controller.hpp"
#include "player.h"
SPI_PSRAM psram;               //
MusicIndex musicIndex(&psram); // Base address for trie
PodPlayer player; // Audio player instance
// Example usage function
void demonstratePathLookup() {
  // Initialize PSRAM controller (assuming it's already set up)

  // Create music index instance

  // Initialize the main trie index
  if (!musicIndex.init("/MUSIC_~1.bin")) {
    Serial.println("Failed to initialize music index");
    return;
  }

  // Initialize the path index
  if (!musicIndex.initPathIndex("/MUSIC_~2.bin")) {
    Serial.println("Failed to initialize path index");
    return;
  }

  // Print statistics
  musicIndex.printStats();

  // Example 1: Search for tracks
  Serial.println("\n=== SEARCHING FOR TRACKS ===");
  search_result_t results[10];
  uint32_t result_count = musicIndex.searchTracks("pourquoi", results, 10);

  Serial.print("Found ");
  Serial.print(result_count);
  Serial.println(" tracks:");

  for (uint32_t i = 0; i < result_count; i++) {
    // Print search result
    musicIndex.printSearchResult(results[i]);

    // Get the file path for this track
    char track_path[256];
    if (musicIndex.getTrackPath(results[i].id, track_path,
                                sizeof(track_path))) {
      Serial.print("  📁 Path: ");
      Serial.println(track_path);
    } else {
      Serial.println("  ❌ Path not found");
    }
    Serial.println();
  }

  // Example 2: Direct path lookup by track ID
  Serial.println("=== DIRECT PATH LOOKUP ===");
  uint32_t test_track_ids[] = {1,   2,  3, 42,
                               233, 999}; // Include some that might not exist

  for (uint32_t i = 0; i < 5; i++) {
    uint32_t track_id = test_track_ids[i];
    char path_buffer[256];

    if (musicIndex.getTrackPath(track_id, path_buffer, sizeof(path_buffer))) {
      Serial.print("Track ");
      Serial.print(track_id);
      Serial.print(": ");
      Serial.println(path_buffer);
    } else {
      Serial.print("Track ");
      Serial.print(track_id);
      Serial.println(": Not found");
    }
  }

  // Example 3: Complete workflow - search, select, play
  Serial.println("\n=== COMPLETE WORKFLOW ===");

  // 1. User searches for "pourquoi"
  search_result_t search_results[5];
  uint32_t found = musicIndex.searchByPrefix("pourquoi", search_results, 5);

  Serial.println("User searches for 'pourquoi':");
  for (uint32_t i = 0; i < found; i++) {
    Serial.print(i + 1);
    Serial.print(". ");
    musicIndex.printSearchResult(search_results[i]);
  }

  // 2. User selects first track result
  for (uint32_t i = 0; i < found; i++) {
    if (search_results[i].type == SEARCH_TRACK) {
      Serial.print("\nUser selects track: ");
      musicIndex.printSearchResult(search_results[i]);

      // 3. Get file path to play
      char file_to_play[256];
      if (musicIndex.getTrackPath(search_results[i].id, file_to_play,
                                  sizeof(file_to_play))) {
        Serial.print("🎵 Now playing: ");
        Serial.println(file_to_play);

        // Here you would:
        // - Open the audio file
        // - Start audio playback
        // - Update UI with track info

      } else {
        Serial.println("❌ Could not get file path");
      }
      break;
    }
  }
  char path_buffer[256];
  musicIndex.getTrackPath(1, path_buffer, sizeof(path_buffer));
//   bool res = player.vs1053.startPlayingFile("Swift\ Guad/Hécatombe\ 2.0/02\ Pourquoi\ _.flac");
  bool res = 
  player.vs1053.startPlayingFile("/track-1.mp3");
  Serial.print("Playing track 233: ");
    if (res) {
        Serial.println("Success");
    } else {
        Serial.println("Failed to play track");
    }
}

// Arduino setup function
void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  Serial.println("Music Index with Path Lookup Example");
  Serial.println("====================================");
  player.setup();
  Serial.println("Initializing PSRAM...");
  if (!psram.init()) {
    Serial.println("PSRAM initialization failed!");
    return;
  }
  // Demonstrate the functionality
  demonstratePathLookup();
}

void loop() {
  // Your main application loop
  delay(5);
}