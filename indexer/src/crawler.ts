import fs from "fs/promises";
import path from "path";
import type { CrawlCallback, CrawlIndex, TrackMetadata } from "./types";
import { parseFile } from "music-metadata";

function getOrAdd<T>(
  map: Map<string, number>,
  reverse: Map<number, string>,
  key: string,
  indexCounter: { value: number }
): number {
  if (!map.has(key)) {
    map.set(key, indexCounter.value);
    reverse.set(indexCounter.value, key);
    return indexCounter.value++;
  }
  return map.get(key)!;
}

export async function crawl(
  root: string,
  callback?: CrawlCallback
): Promise<string[]> {
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
  return files;
}

export async function organizeFiles(messyRoot: string, organizedRoot: string) {
  await crawl(messyRoot, async (filename, rel, fullPath) => {
    const ext = path.extname(filename).toLowerCase();
    if (![".mp3", ".flac", ".ogg", ".wav", ".m4a", ".aac"].includes(ext))
      return;

    const { common } = await parseFile(fullPath);
    const artist = common.artist || "Unknown Artist";
    const album = common.album || "Unknown Album";
    const title = common.title || path.basename(filename, ext);

    const artistDir = path.join(organizedRoot, artist);
    const albumDir = path.join(artistDir, album);
    await fs.mkdir(albumDir, { recursive: true });

    const newFilePath = path.join(albumDir, `${title}${ext}`);
    await fs.rename(fullPath, newFilePath);
  });
}

export async function readMetadata(root: string): Promise<CrawlIndex> {
  const audioExts = new Set([".mp3", ".flac", ".ogg", ".wav", ".m4a", ".aac"]);

  const indexToTrack = new Map<number, string>();
  const indexToArtist = new Map<number, string>();
  const indexToAlbum = new Map<number, string>();
  const indexToGenre = new Map<number, string>();
  const indexToPath = new Map<number, string>();

  const trackToIndex = new Map<string, number>();
  const artistToIndex = new Map<string, number>();
  const albumToIndex = new Map<string, number>();
  const genreToIndex = new Map<string, number>();

  const artistToAlbums = new Map<number, Set<number>>();
  const albumToTracks = new Map<number, Set<number>>();
  const artistToTracks = new Map<number, Set<number>>();
  const genreToTracks = new Map<number, Set<number>>();
  const genreToAlbums = new Map<number, Set<number>>();
  const genreToArtists = new Map<number, Set<number>>();

  const metadataByTrackIndex = new Map<number, TrackMetadata>();
  const trackIndexCounter = { value: 0 };
  const artistIndexCounter = { value: 0 };
  const albumIndexCounter = { value: 0 };
  const genreIndexCounter = { value: 0 };

  await crawl(root, async (filename, rel, fullPath) => {
    const ext = path.extname(filename).toLowerCase();
    if (!audioExts.has(ext)) return;

    // console.log(`Reading metadata for ${rel}/${filename}`);
    const { common, format } = await parseFile(fullPath);

    const title = common.title || path.basename(filename);
    const artist = common.artist || "Unknown Artist";
    const album = common.album || "Unknown Album";
    const genre = common.genre?.[0] || "Unknown Genre";
    const index = (common.track && common.track.no) || 0;
    const year = common.year || 0;
    const duration = format.duration || 0;

    const artistId = getOrAdd(
      artistToIndex,
      indexToArtist,
      artist,
      artistIndexCounter
    );
    const albumId = getOrAdd(
      albumToIndex,
      indexToAlbum,
      album,
      albumIndexCounter
    );
    const genreId = getOrAdd(
      genreToIndex,
      indexToGenre,
      genre,
      genreIndexCounter
    );
    const trackId = trackIndexCounter.value++;

    trackToIndex.set(title, trackId);
    indexToTrack.set(trackId, title);
    indexToPath.set(trackId, `${rel}/${filename}`);

    metadataByTrackIndex.set(trackId, {
      title,
      artist,
      album,
      genre,
      index,
      year,
      duration,
    });

    if (!artistToTracks.has(artistId)) artistToTracks.set(artistId, new Set());
    artistToTracks.get(artistId)!.add(trackId);

    if (!albumToTracks.has(albumId)) albumToTracks.set(albumId, new Set());
    albumToTracks.get(albumId)!.add(trackId);

    if (!artistToAlbums.has(artistId)) artistToAlbums.set(artistId, new Set());
    artistToAlbums.get(artistId)!.add(albumId);

    if (!genreToTracks.has(genreId)) genreToTracks.set(genreId, new Set());
    genreToTracks.get(genreId)!.add(trackId);

    if (!genreToAlbums.has(genreId)) genreToAlbums.set(genreId, new Set());
    genreToAlbums.get(genreId)!.add(albumId);

    if (!genreToArtists.has(genreId)) genreToArtists.set(genreId, new Set());
    genreToArtists.get(genreId)!.add(artistId);
  });

  // Optionally, you can serialize this to a JSON or binary format here
  return {
    indexToTrack,
    indexToArtist,
    indexToAlbum,
    indexToGenre,
    indexToPath,
    artistToAlbums,
    metadataByTrackIndex,
    albumToTracks,
    artistToTracks,
    genreToAlbums,
    genreToTracks,
    genreToArtists,
  };
}

export function serialize(indexes: CrawlIndex): string {
  const data: Record<string, any> = {
    indexToTrack: Array.from(indexes.indexToTrack.entries()),
    indexToArtist: Array.from(indexes.indexToArtist.entries()),
    indexToAlbum: Array.from(indexes.indexToAlbum.entries()),
    indexToGenre: Array.from(indexes.indexToGenre.entries()),
    indexToPath: Array.from(indexes.indexToPath.entries()),
    artistToAlbums: Array.from(
      Array.from(indexes.artistToAlbums.entries()).map(([k, v]) => [
        k,
        Array.from(v),
      ])
    ),
    albumToTracks: Array.from(
      Array.from(indexes.albumToTracks.entries()).map(([k, v]) => [
        k,
        Array.from(v),
      ])
    ),
    artistToTracks: Array.from(
      Array.from(indexes.artistToTracks.entries()).map(([k, v]) => [
        k,
        Array.from(v),
      ])
    ),
    genreToAlbums: Array.from(
      Array.from(indexes.genreToAlbums.entries()).map(([k, v]) => [
        k,
        Array.from(v),
      ])
    ),
    genreToTracks: Array.from(
      Array.from(indexes.genreToTracks.entries()).map(([k, v]) => [
        k,
        Array.from(v),
      ])
    ),
    genreToArtists: Array.from(
      Array.from(indexes.genreToArtists.entries()).map(([k, v]) => [
        k,
        Array.from(v),
      ])
    ),
    metadataByTrackIndex: Array.from(indexes.metadataByTrackIndex.entries()),
  };
  return JSON.stringify(data, null, 2);
}

export function unserialize(data: string): CrawlIndex {
  const parsed: Record<string, any> = JSON.parse(data);
  const indexToTrack = new Map<number, string>(parsed.indexToTrack);
  const indexToArtist = new Map<number, string>(parsed.indexToArtist);
  const indexToAlbum = new Map<number, string>(parsed.indexToAlbum);
  const indexToGenre = new Map<number, string>(parsed.indexToGenre);
  const indexToPath = new Map<number, string>(parsed.indexToPath);
  const artistToAlbums = new Map<number, Set<number>>(
    parsed.artistToAlbums.map(([k, v]: [number, number[]]) => [k, new Set(v)])
  );
  const albumToTracks = new Map<number, Set<number>>(
    parsed.albumToTracks.map(([k, v]: [number, number[]]) => [k, new Set(v)])
  );
  const artistToTracks = new Map<number, Set<number>>(
    parsed.artistToTracks.map(([k, v]: [number, number[]]) => [k, new Set(v)])
  );
  const genreToAlbums = new Map<number, Set<number>>(
    parsed.genreToAlbums.map(([k, v]: [number, number[]]) => [k, new Set(v)])
  );
  const genreToTracks = new Map<number, Set<number>>(
    parsed.genreToTracks.map(([k, v]: [number, number[]]) => [k, new Set(v)])
  );
  const genreToArtists = new Map<number, Set<number>>(
    parsed.genreToArtists
      ? parsed.genreToArtists.map(([k, v]: [number, number[]]) => [
          k,
          new Set(v),
        ])
      : []
  );
  const metadataByTrackIndex = new Map<number, TrackMetadata>(
    parsed.metadataByTrackIndex.map(
      ([k, v]: [
        number,
        {
          title: string;
          artist: string;
          album: string;
          genre: string;
          index: number;
          year: number;
          duration: number;
        }
      ]) => [k, v]
    )
  );
  return {
    indexToTrack,
    indexToArtist,
    indexToAlbum,
    indexToGenre,
    indexToPath,
    artistToAlbums,
    albumToTracks,
    artistToTracks,
    genreToAlbums,
    genreToTracks,
    genreToArtists,
    metadataByTrackIndex,
  };
}
export function printIndexes(indexes: CrawlIndex) {
  console.log("Tracks:");
  for (const [index, track] of indexes.indexToTrack.entries()) {
    console.log(`  ${index}: ${track}`);
  }

  console.log("Artists:");
  for (const [index, artist] of indexes.indexToArtist.entries()) {
    console.log(`  ${index}: ${artist}`);
  }

  console.log("Albums:");
  for (const [index, album] of indexes.indexToAlbum.entries()) {
    console.log(`  ${index}: ${album}`);
  }

  console.log("Genres:");
  for (const [index, genre] of indexes.indexToGenre.entries()) {
    console.log(`  ${index}: ${genre}`);
  }
  console.log("Paths:");
  for (const [index, path] of indexes.indexToPath.entries()) {
    console.log(`  ${index}: ${path}`);
  }
  console.log("Metadata by Track Index:");
  for (const [index, metadata] of indexes.metadataByTrackIndex.entries()) {
    console.log(
      `  ${index}: ${metadata.index} ${metadata.title} by ${metadata.artist} from ${metadata.album} (${metadata.year})`
    );
  }
  console.log("Artist to Albums:");
  for (const [artistIndex, albumSet] of indexes.artistToAlbums.entries()) {
    const albums = Array.from(albumSet)
      .map((albumId) => indexes.indexToAlbum.get(albumId))
      .filter((a) => a !== undefined);
    console.log(`  Artist ${artistIndex}: ${albums.join(", ")}`);
  }
  console.log("Album to Tracks:");
  for (const [albumIndex, trackSet] of indexes.albumToTracks.entries()) {
    const tracks = Array.from(trackSet)
      .map((trackId) => indexes.indexToTrack.get(trackId))
      .filter((t) => t !== undefined);
    console.log(`  Album ${albumIndex}: ${tracks.join(", ")}`);
  }
  console.log("Artist to Tracks:");
  for (const [artistIndex, trackSet] of indexes.artistToTracks.entries()) {
    const tracks = Array.from(trackSet)
      .map((trackId) => indexes.indexToTrack.get(trackId))
      .filter((t) => t !== undefined);
    console.log(`  Artist ${artistIndex}: ${tracks.join(", ")}`);
  }
  console.log("Genre to Albums:");
  for (const [genreIndex, albumSet] of indexes.genreToAlbums.entries()) {
    const albums = Array.from(albumSet)
      .map((albumId) => indexes.indexToAlbum.get(albumId))
      .filter((a) => a !== undefined);
    console.log(`  Genre ${genreIndex}: ${albums.join(", ")}`);
  }
  console.log("Genre to Tracks:");
  for (const [genreIndex, trackSet] of indexes.genreToTracks.entries()) {
    const tracks = Array.from(trackSet)
      .map((trackId) => indexes.indexToTrack.get(trackId))
      .filter((t) => t !== undefined);
    console.log(`  Genre ${genreIndex}: ${tracks.join(", ")}`);
  }
  console.log("Index to Path:");
  for (const [index, filePath] of indexes.indexToPath.entries()) {
    console.log(`  ${index}: ${filePath}`);
  }
  console.log("Finished printing indexes.");
}
