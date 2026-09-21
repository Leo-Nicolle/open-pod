// Helper function to normalize strings for search
export function normalizeString(
  str: string,
  caseSensitive: boolean = false
): string {
  let normalized = str.trim();
  if (!caseSensitive) {
    normalized = normalized.toLowerCase();
  }

  // Remove accents and convert to ASCII-safe characters
  normalized = removeAccents(normalized);

  // Remove common articles and prepositions for better matching
  normalized = normalized.replace(/^(the|a|an|le|la|les|un|une|des)\s+/i, "");

  // Ensure only ASCII characters remain
  normalized = normalized.replace(/[^\x00-\x7F]/g, "");

  return normalized;
}

// Function to remove accents and convert to ASCII equivalents
export function removeAccents(str: string): string {
  const accentMap: { [key: string]: string } = {
    à: "a",
    á: "a",
    â: "a",
    ã: "a",
    ä: "a",
    å: "a",
    æ: "ae",
    ç: "c",
    è: "e",
    é: "e",
    ê: "e",
    ë: "e",
    ì: "i",
    í: "i",
    î: "i",
    ï: "i",
    ñ: "n",
    ò: "o",
    ó: "o",
    ô: "o",
    õ: "o",
    ö: "o",
    ø: "o",
    œ: "oe",
    ù: "u",
    ú: "u",
    û: "u",
    ü: "u",
    ý: "y",
    ÿ: "y",
    À: "A",
    Á: "A",
    Â: "A",
    Ã: "A",
    Ä: "A",
    Å: "A",
    Æ: "AE",
    Ç: "C",
    È: "E",
    É: "E",
    Ê: "E",
    Ë: "E",
    Ì: "I",
    Í: "I",
    Î: "I",
    Ï: "I",
    Ñ: "N",
    Ò: "O",
    Ó: "O",
    Ô: "O",
    Õ: "O",
    Ö: "O",
    Ø: "O",
    Œ: "OE",
    Ù: "U",
    Ú: "U",
    Û: "U",
    Ü: "U",
    Ý: "Y",
    Ÿ: "Y",
  };

  return str.replace(
    /[àáâãäåæçèéêëìíîïñòóôõöøœùúûüýÿÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÑÒÓÔÕÖØŒÙÚÛÜÝŸ]/g,
    (match) => accentMap[match] || match
  );
}

// Convert a display name to ASCII (remove accents, drop non-ASCII) while
// preserving case. Used for names that will be rendered with the embedded
// ASCII-only font and for the string indexes loaded by the firmware.
export function sanitizeName(str: string): string {
  return removeAccents(str)
    .replace(/[^\x20-\x7E]/g, "")
    .replace(/\s+/g, " ")
    .trim();
}

// Splits a combined artist tag ("A & B", "A feat. B", "A, B, C") into its
// individual artist names, so each performer is indexed - and browsable - on
// their own instead of every combination ("A & B", "A & B & C", "A & C", ...)
// becoming its own noisy, one-off artist entry. Case-insensitive de-duped and
// each name run through sanitizeName. Returns [] for an empty/missing tag -
// callers decide the "no artist" fallback.
// feat./ft. use a lookahead instead of a trailing \b: with the period
// consumed, the position right after it sits between two non-word
// characters ("." and the following space), so \b would never match there.
const ARTIST_SEPARATOR_REGEX =
  /\s*(?:\/|;|,|&|\bfeat\.?(?=\s|$)|\bft\.?(?=\s|$)|\bfeaturing\b|\bwith\b)\s*/i;

export function splitArtists(rawArtist: string | undefined | null): string[] {
  if (!rawArtist) return [];

  const seen = new Set<string>();
  const result: string[] = [];
  for (const part of rawArtist.split(ARTIST_SEPARATOR_REGEX)) {
    const name = sanitizeName(part);
    if (name.length === 0) continue;
    const key = name.toLowerCase();
    if (seen.has(key)) continue;
    seen.add(key);
    result.push(name);
  }
  return result;
}
