export type TrieNode = {
  key: string; // segment du préfixe (ex: "beat", "les")
  children: TrieNode[]; // enfants
  results?: SearchResult[]; // défini uniquement en feuille
};

export type SearchResult = {
  type: "track" | "artist" | "album" | "genre";
  id: number;
  name: string;
  relevance: number; // 0-100, higher = more relevant
};

export type SerializedResult = {
  type: number; // 0=track, 1=artist, 2=album, 3=genre
  id: number;
  nameOffset: number;
  nameLength: number;
  relevance: number;
};

export type SerializedNode = {
  keyOffset: number; // dans string pool
  keyLength: number;
  childCount: number;
  firstChildOffset: number; // dans la table de noeuds
  resultCount: number;
  firstResultOffset: number; // dans la table de résultats
};

export type SerializedTrie = {
  stringPool: string;
  stringOffsets: number[];
  nodes: SerializedNode[];
  results: SerializedResult[];
};

// Configuration for trie building
export type TrieConfig = {
  includePartialMatches: boolean; // Include partial word matches
  caseSensitive: boolean;
  minPrefixLength: number; // Minimum prefix length to index
  maxResults: number; // Maximum results per node
};

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
  genreToArtists: Map<number, Set<number>>;
};

// Path index types for track path lookup
export type PathIndex = {
  trackPaths: Map<number, string>; // trackId -> filePath
};

// Types for serialized data
export type SerializedStringIndex = {
  entryCount: number;
  stringData: string;
  stringOffsets: number[];
  ids: number[];
};

export type SerializedRelationshipMap = {
  entryCount: number;
  totalTargetCount: number;
  sourceIds: number[];
  targetCounts: number[];
  targetIds: number[];
};

// Legacy type for compatibility
export type SerializedPathIndex = SerializedStringIndex & {
  trackCount: number;
  pathData: string;
  pathOffsets: number[];
  trackIds: number[];
};
