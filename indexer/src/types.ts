export type Config = {
  maxDistance?: number; // Maximum Levenshtein distance for fuzzy search
  prefixBoost?: number; // Boost for prefix matches
  phonetic?: boolean; // Enable phonetic matching
  caseSensitive?: boolean; // Case sensitivity
  minQueryLength?: number; // Minimum query length for fuzzy search
  maxResults?: number; // Maximum results to return
};

export type PhoneticMap = Record<string, string>; // Maps phonetic codes to words
export type CrawlCallback = (
  filename: string,
  relative: string,
  path: string
) => Promise<void>;

export type TrackMetadata = {
  title: string;
  artist: string;
  album: string;
  genre: string;
  year: number;
  index: number; // Index in the crawl index
  duration: number; // en secondes
};
export type CrawlIndex = {
  indexToTrack: Map<number, string>;
  indexToArtist: Map<number, string>;
  indexToAlbum: Map<number, string>;
  indexToGenre: Map<number, string>;
  indexToPath: Map<number, string>;
  metadataByTrackIndex: Map<number, TrackMetadata>;
  artistToAlbums: Map<number, Set<number>>;
  albumToTracks: Map<number, Set<number>>;
  artistToTracks: Map<number, Set<number>>;
  genreToAlbums: Map<number, Set<number>>;
  genreToTracks: Map<number, Set<number>>;
};
