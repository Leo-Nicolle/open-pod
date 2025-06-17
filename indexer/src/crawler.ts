import fs from "fs/promises";
import path from "path";
import type { CrawlCallback } from "./types";
import { parseFile } from "music-metadata";

export async function crawl(
  root: string,
  callback?: CrawlCallback
): Promise<string[]> {
  // recursive function to crawl directories and collect file paths
  const Q = [root];
  const files: string[] = [];
  while (Q.length > 0) {
    const current = Q.pop();
    if (!current) continue;
    const stat = await fs.stat(current);
    if (stat.isDirectory()) {
      const entries = await fs.readdir(current);
      for (const entry of entries) {
        Q.push(path.join(current, entry));
      }
    } else if (stat.isFile()) {
      files.push(current);
      const folder = path.dirname(current);
      const relativePath = path.relative(root, folder);
      await callback?.(path.basename(current), relativePath, current);
    }
  }
  return Promise.resolve(files);
}
export async function readMetadata(
  root: string
): Promise<Record<string, any>[]> {
  // read metadata from files in the directory
  const audio = new Set<string>([
    ".mp3",
    ".flac",
    ".ogg",
    ".wav",
    ".m4a",
    ".aac",
  ]);
  const indexToTrack: Map<number, string> = new Map();
  const indexToArtist: Map<number, string> = new Map();
  const indexToAlbum: Map<number, string> = new Map();
  const indexToGenre: Map<number, string> = new Map();
  const indexToPath: Map<number, string> = new Map();
  const trackToIndex: Map<string, number> = new Map();
  const artistToTracks: Map<string, number[]> = new Map();
  const albumToTracks: Map<string, number[]> = new Map();
  const artistToAlbums: Map<string, number[]> = new Map();
  const genreToAlbums: Map<string, number[]> = new Map();
  const genreToTracks: Map<string, number[]> = new Map();
  let trackIndex = 0;
  let artistIndex = 0;
  let albumIndex = 0;
  let genreIndex = 0;
  await crawl(root, async (filename: string, rel: string, fullPath: string) => {
    const extension = path.extname(filename).toLowerCase();
    if (!audio.has(extension)) return;
    console.log(`Reading metadata for ${rel}/${filename}`);
    const { common } = await parseFile(fullPath);
    let { title, artist, album, year } = common;
    if (!title) {
      title = "Unknown Title";
    }
    if (!artist) {
      artist = "Unknown Artist";
    }
    if (!album) {
      album = "Unknown Album";
    }
    if (!year) {
      year = 0;
    }
    trackToIndex.set(title, trackIndex);
    indexToPath.set(trackIndex, fullPath);
    indexToTrack.set(trackIndex, title);
    trackIndex++;
    art;
    // artistToTracks.set(artist,
  });
}
