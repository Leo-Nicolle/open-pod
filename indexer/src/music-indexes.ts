import type {
  CrawlIndex,
  SerializedPathIndex,
  SerializedRelationshipMap,
  SerializedStringIndex,
} from "./types";

export function exportStringIndexToBinary(
  indexMap: Map<number, string>
): Uint8Array {
  const ids: number[] = [];
  const strings: Uint8Array[] = [];

  // Sort entries by ID for consistent ordering
  const sortedEntries = Array.from(indexMap.entries()).sort(
    (a, b) => a[0] - b[0]
  );

  const encoder = new TextEncoder();
  const offsets: number[] = [];
  offsets.push(0);
  let stringsLength = 0;

  // Process each entry
  for (const [id, str] of sortedEntries) {
    ids.push(id);
    const encoded = encoder.encode(`${str}\0`); // Null-terminated strings
    strings.push(encoded);
    stringsLength += encoded.byteLength;
    offsets.push(stringsLength);
  }

  // Calculate total size needed
  const headerSize = 8; // 2 uint32 values (entryCount, stringDataSize)
  const idsSize = ids.length * 4; // 4 bytes per uint32
  const offsetsSize = offsets.length * 4; // 4 bytes per uint32
  const totalSize = headerSize + idsSize + offsetsSize + stringsLength;

  // Create and populate buffer
  const buffer = new ArrayBuffer(totalSize);
  const view = new DataView(buffer);
  let offset = 0;

  // Write header
  view.setUint32(offset, ids.length, true); // Entry count
  offset += 4;
  view.setUint32(offset, stringsLength, true); // String data size
  offset += 4;

  // Write IDs
  for (const id of ids) {
    view.setUint32(offset, id, true);
    offset += 4;
  }

  // Write string offsets
  for (const stringOffset of offsets) {
    view.setUint32(offset, stringOffset, true);
    offset += 4;
  }

  // Write string data
  for (const stringBytes of strings) {
    for (let j = 0; j < stringBytes.byteLength; j++) {
      view.setUint8(offset, stringBytes[j]);
      offset++;
    }
  }

  return new Uint8Array(buffer);
}

export function exportRelationshipMapToBinary(
  relationshipMap: Map<number, Set<number>>
): Uint8Array {
  const sourceIds: number[] = [];
  const targetCounts: number[] = [];
  const targetIds: number[] = [];

  // Sort entries by source ID for consistent ordering
  const sortedEntries = Array.from(relationshipMap.entries()).sort(
    (a, b) => a[0] - b[0]
  );

  // Process each entry
  for (const [sourceId, targetSet] of sortedEntries) {
    sourceIds.push(sourceId);
    const sortedTargets = Array.from(targetSet).sort((a, b) => a - b);
    targetCounts.push(sortedTargets.length);
    targetIds.push(...sortedTargets);
  }

  // Calculate total size needed
  const headerSize = 8; // 2 uint32 values (entryCount, totalTargetCount)
  const sourceIdsSize = sourceIds.length * 4; // 4 bytes per uint32
  const targetCountsSize = targetCounts.length * 4; // 4 bytes per uint32
  const targetIdsSize = targetIds.length * 4; // 4 bytes per uint32
  const totalSize =
    headerSize + sourceIdsSize + targetCountsSize + targetIdsSize;

  // Create and populate buffer
  const buffer = new ArrayBuffer(totalSize);
  const view = new DataView(buffer);
  let offset = 0;

  // Write header
  view.setUint32(offset, sourceIds.length, true); // Entry count
  offset += 4;
  view.setUint32(offset, targetIds.length, true); // Total target count
  offset += 4;

  // Write source IDs
  for (const sourceId of sourceIds) {
    view.setUint32(offset, sourceId, true);
    offset += 4;
  }

  // Write target counts
  for (const count of targetCounts) {
    view.setUint32(offset, count, true);
    offset += 4;
  }

  // Write target IDs
  for (const targetId of targetIds) {
    view.setUint32(offset, targetId, true);
    offset += 4;
  }

  return new Uint8Array(buffer);
}

// Generic import function for string indexes
export function importStringIndexFromBinary(
  buffer: Uint8Array
): SerializedStringIndex {
  const view = new DataView(
    buffer.buffer,
    buffer.byteOffset,
    buffer.byteLength
  );
  let offset = 0;

  // Read header
  const entryCount = view.getUint32(offset, true);
  offset += 4;
  const stringDataSize = view.getUint32(offset, true);
  offset += 4;

  // Read IDs
  const ids: number[] = [];
  for (let i = 0; i < entryCount; i++) {
    ids.push(view.getUint32(offset, true));
    offset += 4;
  }

  // Read string offsets (note: there's one extra offset for the end)
  const stringOffsets: number[] = [];
  for (let i = 0; i <= entryCount; i++) {
    stringOffsets.push(view.getUint32(offset, true));
    offset += 4;
  }

  // Read string data
  const stringDataBytes = buffer.slice(offset, offset + stringDataSize);
  const stringData = new TextDecoder().decode(stringDataBytes);

  return {
    entryCount,
    stringData,
    stringOffsets,
    ids,
  };
}

// Generic import function for relationship maps
export function importRelationshipMapFromBinary(
  buffer: Uint8Array
): SerializedRelationshipMap {
  const view = new DataView(
    buffer.buffer,
    buffer.byteOffset,
    buffer.byteLength
  );
  let offset = 0;

  // Read header
  const entryCount = view.getUint32(offset, true);
  offset += 4;
  const totalTargetCount = view.getUint32(offset, true);
  offset += 4;

  // Read source IDs
  const sourceIds: number[] = [];
  for (let i = 0; i < entryCount; i++) {
    sourceIds.push(view.getUint32(offset, true));
    offset += 4;
  }

  // Read target counts
  const targetCounts: number[] = [];
  for (let i = 0; i < entryCount; i++) {
    targetCounts.push(view.getUint32(offset, true));
    offset += 4;
  }

  // Read target IDs
  const targetIds: number[] = [];
  for (let i = 0; i < totalTargetCount; i++) {
    targetIds.push(view.getUint32(offset, true));
    offset += 4;
  }

  return {
    entryCount,
    totalTargetCount,
    sourceIds,
    targetCounts,
    targetIds,
  };
}

// Convenience functions for specific imports
export function importArtistIndexFromBinary(
  buffer: Uint8Array
): SerializedStringIndex {
  return importStringIndexFromBinary(buffer);
}

export function importAlbumIndexFromBinary(
  buffer: Uint8Array
): SerializedStringIndex {
  return importStringIndexFromBinary(buffer);
}

export function importGenreIndexFromBinary(
  buffer: Uint8Array
): SerializedStringIndex {
  return importStringIndexFromBinary(buffer);
}

export function importTrackIndexFromBinary(
  buffer: Uint8Array
): SerializedStringIndex {
  return importStringIndexFromBinary(buffer);
}

export function importArtistToAlbumsFromBinary(
  buffer: Uint8Array
): SerializedRelationshipMap {
  return importRelationshipMapFromBinary(buffer);
}

export function importAlbumToTracksFromBinary(
  buffer: Uint8Array
): SerializedRelationshipMap {
  return importRelationshipMapFromBinary(buffer);
}

export function importArtistToTracksFromBinary(
  buffer: Uint8Array
): SerializedRelationshipMap {
  return importRelationshipMapFromBinary(buffer);
}

export function importGenreToAlbumsFromBinary(
  buffer: Uint8Array
): SerializedRelationshipMap {
  return importRelationshipMapFromBinary(buffer);
}

export function importGenreToTracksFromBinary(
  buffer: Uint8Array
): SerializedRelationshipMap {
  return importRelationshipMapFromBinary(buffer);
}

// Legacy compatibility function
export function importPathIndexFromBinary(
  buffer: Uint8Array
): SerializedPathIndex {
  const base = importStringIndexFromBinary(buffer);
  return {
    ...base,
    trackCount: base.entryCount,
    pathData: base.stringData,
    pathOffsets: base.stringOffsets,
    trackIds: base.ids,
  };
}

export function exportIndexesToBinary(crawlIndex: CrawlIndex) {
  return {
    genre_to_tracks: exportRelationshipMapToBinary(crawlIndex.genreToTracks),
    genre_to_artists: exportRelationshipMapToBinary(crawlIndex.genreToArtists),
    genre_to_albums: exportRelationshipMapToBinary(crawlIndex.genreToAlbums),
    artist_to_tracks: exportRelationshipMapToBinary(crawlIndex.artistToTracks),
    artist_to_albums: exportRelationshipMapToBinary(crawlIndex.artistToAlbums),
    album_to_tracks: exportRelationshipMapToBinary(crawlIndex.albumToTracks),
    artist_index: exportStringIndexToBinary(crawlIndex.indexToArtist),
    album_index: exportStringIndexToBinary(crawlIndex.indexToAlbum),
    genre_index: exportStringIndexToBinary(crawlIndex.indexToGenre),
    track_index: exportStringIndexToBinary(crawlIndex.indexToTrack),
    path_index: exportStringIndexToBinary(crawlIndex.indexToPath),
  };
}
