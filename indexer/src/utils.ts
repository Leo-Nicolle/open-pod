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
