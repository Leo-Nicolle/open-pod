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
