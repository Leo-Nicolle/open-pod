import type { CrawlIndex, SerializedPathIndex } from "./types";
export function exportPathIndexToBinary(crawlIndex: CrawlIndex): Uint8Array {
  const trackIds: number[] = [];
  const paths: Uint8Array[] = [];
  const indexToPath = crawlIndex.indexToPath;
  const sortedEntries = Array.from(indexToPath.entries()).sort(
    (a, b) => a[0] - b[0]
  );
  const encoder = new TextEncoder();
  const offsets: number[] = [];
  offsets.push(0);
  let pathsLength = 0;
  for (const [trackId, path] of sortedEntries) {
    trackIds.push(trackId);
    const encoded = encoder.encode(`${path}\0`);
    paths.push(encoded);
    pathsLength += encoded.byteLength;
    offsets.push(pathsLength);
  }
  // Calculate total size needed
  const headerSize = 8; // 2 uint32 values (trackCount, pathDataSize)
  const trackIdsSize = trackIds.length * 4; // 4 bytes per uint32
  const pathOffsetsSize = offsets.length * 4; // 4 bytes per uint32

  const totalSize = headerSize + trackIdsSize + pathOffsetsSize + pathsLength;

  const buffer = new ArrayBuffer(totalSize);
  const view = new DataView(buffer);
  let offset = 0;

  // Write header
  view.setUint32(offset, trackIds.length, true);
  offset += 4;
  view.setUint32(offset, pathsLength, true);
  offset += 4;

  // Write track IDs
  for (let i = 0; i < trackIds.length; i++) {
    view.setUint32(offset, trackIds[i], true);
    offset += 4;
  }

  // Write path offsets
  for (let i = 0; i < offsets.length; i++) {
    view.setUint32(offset, offsets[i], true);
    offset += 4;
  }
  for (let i = 0; i < paths.length; i++) {
    for (let j = 0; j < paths[i].byteLength; j++) {
      view.setUint8(offset, paths[i][j]);
      offset++;
    }
  }
  return new Uint8Array(buffer);
}

export function importPathIndexFromBinary(
  buffer: Uint8Array
): SerializedPathIndex {
  const view = new DataView(
    buffer.buffer,
    buffer.byteOffset,
    buffer.byteLength
  );
  let offset = 0;

  // Read header
  const trackCount = view.getUint32(offset, true);
  offset += 4;
  const pathDataSize = view.getUint32(offset, true);
  offset += 4;

  // Read track IDs
  const trackIds: number[] = [];
  for (let i = 0; i < trackCount; i++) {
    trackIds.push(view.getUint32(offset, true));
    offset += 4;
  }

  // Read path offsets
  const pathOffsets: number[] = [];
  for (let i = 0; i < trackCount; i++) {
    pathOffsets.push(view.getUint32(offset, true));
    offset += 4;
  }

  // Read path data
  const pathDataBytes = buffer.slice(offset, offset + pathDataSize);
  const pathData = new TextDecoder().decode(pathDataBytes);

  return {
    trackCount,
    pathData,
    pathOffsets,
    trackIds,
  };
}

// Utility function to lookup path by track ID from serialized path index
export function lookupTrackPath(
  serialized: SerializedPathIndex,
  trackId: number
): string | null {
  // Binary search for track ID
  let left = 0;
  let right = serialized.trackIds.length - 1;

  while (left <= right) {
    const mid = Math.floor((left + right) / 2);
    const midTrackId = serialized.trackIds[mid];

    if (midTrackId === trackId) {
      // Found the track ID, extract the path
      const pathOffset = serialized.pathOffsets[mid];
      const nextOffset =
        mid + 1 < serialized.pathOffsets.length
          ? serialized.pathOffsets[mid + 1] - 1 // -1 to exclude null terminator
          : serialized.pathData.length;

      return serialized.pathData.slice(pathOffset, nextOffset);
    } else if (midTrackId < trackId) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }

  return null; // Track ID not found
}
