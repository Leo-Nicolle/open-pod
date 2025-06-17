import { readMetadata } from "./crawler";
import { searchTrie, createMusicIndexDemo } from "./trie";
import fs from "fs/promises";

async function main() {
  // Example usage of the updated trie system
  console.log("=== Music Index Trie Demo ===\n");

  // Step 1: Crawl a music directory (replace with your actual music path)
  const musicPath = process.argv[2] || "./test-music";

  if (
    !(await fs
      .access(musicPath)
      .then(() => true)
      .catch(() => false))
  ) {
    console.log(`Music directory '${musicPath}' not found.`);
    console.log("Usage: npm run example <music-directory>");
    console.log("Creating a demo with mock data instead...\n");

    // Create mock crawl index for demonstration
    const mockCrawlIndex = createMockCrawlIndex();
    await demonstrateTrie(mockCrawlIndex);
    return;
  }

  console.log(`Crawling music directory: ${musicPath}`);
  const crawlIndex = await readMetadata(musicPath);

  console.log(`Found:`);
  console.log(`- ${crawlIndex.indexToTrack.size} tracks`);
  console.log(`- ${crawlIndex.indexToArtist.size} artists`);
  console.log(`- ${crawlIndex.indexToAlbum.size} albums`);
  console.log(`- ${crawlIndex.indexToGenre.size} genres\n`);

  await demonstrateTrie(crawlIndex);
}

async function demonstrateTrie(crawlIndex: any) {
  // Step 2: Build and serialize the trie
  const demo = createMusicIndexDemo(crawlIndex);

  console.log("\n=== Search Demo ===");

  // Step 3: Demonstrate search functionality
  const searchQueries = ["beat", "rock", "the", "love", "john"];

  for (const query of searchQueries) {
    console.log(`\nSearching for: "${query}"`);
    const results = searchTrie(demo.trie, query, 5);

    if (results.length === 0) {
      console.log("  No results found");
    } else {
      results.forEach((result, index) => {
        console.log(
          `  ${index + 1}. [${result.type.toUpperCase()}] ${
            result.name
          } (relevance: ${result.relevance})`
        );
      });
    }
  }

  // Step 4: Save files for STM32
  console.log("\n=== Saving STM32 Files ===");

  // Save binary data
  const binaryPath = "./music_index.bin";
  await fs.writeFile(binaryPath, demo.binaryData);
  console.log(`Binary data saved to: ${binaryPath}`);

  // Save C header
  const headerPath = "./music_index.h";
  await fs.writeFile(headerPath, demo.cHeader);
  console.log(`C header saved to: ${headerPath}`);

  // Save C source file with the binary data
  const sourcePath = "./music_index.c";
  const sourceContent = generateCSource(demo.binaryData);
  await fs.writeFile(sourcePath, sourceContent);
  console.log(`C source saved to: ${sourcePath}`);

  console.log("\n=== STM32 Integration Ready! ===");
  console.log("Files generated for your STM32 project:");
  console.log(`- ${headerPath} (include in your project)`);
  console.log(`- ${sourcePath} (compile with your project)`);
  console.log(`- ${binaryPath} (for reference/debugging)`);
}

function createMockCrawlIndex() {
  // Create a simple mock index for demonstration
  const indexToTrack = new Map([
    [0, "Bohemian Rhapsody"],
    [1, "Stairway to Heaven"],
    [2, "Hotel California"],
    [3, "Sweet Child O' Mine"],
    [4, "Imagine"],
    [5, "Like a Rolling Stone"],
    [6, "Billie Jean"],
    [7, "Hey Jude"],
    [8, "Purple Haze"],
    [9, "Good Vibrations"],
  ]);

  const indexToArtist = new Map([
    [0, "Queen"],
    [1, "Led Zeppelin"],
    [2, "Eagles"],
    [3, "Guns N' Roses"],
    [4, "John Lennon"],
    [5, "Bob Dylan"],
    [6, "Michael Jackson"],
    [7, "The Beatles"],
    [8, "Jimi Hendrix"],
    [9, "The Beach Boys"],
  ]);

  const indexToAlbum = new Map([
    [0, "A Night at the Opera"],
    [1, "Led Zeppelin IV"],
    [2, "Hotel California"],
    [3, "Appetite for Destruction"],
    [4, "Imagine"],
    [5, "Highway 61 Revisited"],
    [6, "Thriller"],
    [7, "Hey Jude"],
    [8, "Are You Experienced"],
    [9, "Pet Sounds"],
  ]);

  const indexToGenre = new Map([
    [0, "Rock"],
    [1, "Classic Rock"],
    [2, "Pop"],
    [3, "Hard Rock"],
    [4, "Folk Rock"],
  ]);

  const indexToPath = new Map([
    [0, "Queen/A Night at the Opera/Bohemian Rhapsody.mp3"],
    [1, "Led Zeppelin/Led Zeppelin IV/Stairway to Heaven.mp3"],
    [2, "Eagles/Hotel California/Hotel California.mp3"],
    [3, "Guns N Roses/Appetite for Destruction/Sweet Child O Mine.mp3"],
    [4, "John Lennon/Imagine/Imagine.mp3"],
    [5, "Bob Dylan/Highway 61 Revisited/Like a Rolling Stone.mp3"],
    [6, "Michael Jackson/Thriller/Billie Jean.mp3"],
    [7, "The Beatles/Hey Jude/Hey Jude.mp3"],
    [8, "Jimi Hendrix/Are You Experienced/Purple Haze.mp3"],
    [9, "The Beach Boys/Pet Sounds/Good Vibrations.mp3"],
  ]);

  // Create simple relationship maps
  const artistToAlbums = new Map();
  const albumToTracks = new Map();
  const artistToTracks = new Map();
  const genreToTracks = new Map();
  const genreToAlbums = new Map();
  const metadataByTrackIndex = new Map();

  // Populate with mock data
  for (let i = 0; i < 10; i++) {
    artistToAlbums.set(i, new Set([i]));
    albumToTracks.set(i, new Set([i]));
    artistToTracks.set(i, new Set([i]));
    genreToTracks.set(i % 5, new Set([i]));
    genreToAlbums.set(i % 5, new Set([i]));

    metadataByTrackIndex.set(i, {
      title: indexToTrack.get(i)!,
      artist: indexToArtist.get(i)!,
      album: indexToAlbum.get(i)!,
      genre: indexToGenre.get(i % 5)!,
      year: 1970 + i * 5,
      index: i + 1,
      duration: 180 + i * 30,
    });
  }

  return {
    indexToTrack,
    indexToArtist,
    indexToAlbum,
    indexToGenre,
    indexToPath,
    artistToAlbums,
    albumToTracks,
    artistToTracks,
    genreToTracks,
    genreToAlbums,
    metadataByTrackIndex,
  };
}

function generateCSource(binaryData: Uint8Array): string {
  let source = `// Auto-generated music index data for STM32\n`;
  source += `// Generated on ${new Date().toISOString()}\n\n`;
  source += `#include "music_index.h"\n\n`;

  // Write binary data as C array
  source += `const uint8_t music_index_data[${binaryData.length}] = {\n`;

  for (let i = 0; i < binaryData.length; i += 16) {
    source += "    ";
    for (let j = 0; j < 16 && i + j < binaryData.length; j++) {
      source += `0x${binaryData[i + j].toString(16).padStart(2, "0")}`;
      if (i + j < binaryData.length - 1) source += ", ";
    }
    source += "\n";
  }

  source += `};\n\n`;
  source += `const uint32_t music_index_size = ${binaryData.length};\n`;

  return source;
}

// if (require.main === module) {
main().catch(console.error);
// }
