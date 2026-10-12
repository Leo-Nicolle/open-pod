//! Name normalisation, ported from `indexer/src/utils.ts`. Output must stay
//! byte-identical to the TS indexer: the firmware fonts are ASCII-only and the
//! search trie keys are built from these strings.

fn accent_replacement(c: char) -> Option<&'static str> {
    Some(match c {
        'à' | 'á' | 'â' | 'ã' | 'ä' | 'å' => "a",
        'æ' => "ae",
        'ç' => "c",
        'è' | 'é' | 'ê' | 'ë' => "e",
        'ì' | 'í' | 'î' | 'ï' => "i",
        'ñ' => "n",
        'ò' | 'ó' | 'ô' | 'õ' | 'ö' | 'ø' => "o",
        'œ' => "oe",
        'ù' | 'ú' | 'û' | 'ü' => "u",
        'ý' | 'ÿ' => "y",
        'À' | 'Á' | 'Â' | 'Ã' | 'Ä' | 'Å' => "A",
        'Æ' => "AE",
        'Ç' => "C",
        'È' | 'É' | 'Ê' | 'Ë' => "E",
        'Ì' | 'Í' | 'Î' | 'Ï' => "I",
        'Ñ' => "N",
        'Ò' | 'Ó' | 'Ô' | 'Õ' | 'Ö' | 'Ø' => "O",
        'Œ' => "OE",
        'Ù' | 'Ú' | 'Û' | 'Ü' => "U",
        'Ý' | 'Ÿ' => "Y",
        _ => return None,
    })
}

pub fn remove_accents(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for c in s.chars() {
        match accent_replacement(c) {
            Some(r) => out.push_str(r),
            None => out.push(c),
        }
    }
    out
}

/// JS `\s`: Unicode White_Space plus U+FEFF.
fn is_js_space(c: char) -> bool {
    c.is_whitespace() || c == '\u{feff}'
}

/// JS `\w` (no `u` flag): ASCII word characters only.
fn is_js_word(c: char) -> bool {
    c.is_ascii_alphanumeric() || c == '_'
}

/// ASCII display name: accents folded, non-printable/non-ASCII dropped,
/// whitespace collapsed and trimmed.
pub fn sanitize_name(s: &str) -> String {
    let ascii: String = remove_accents(s)
        .chars()
        .filter(|c| (' '..='~').contains(c))
        .collect();
    // Only ASCII 0x20..0x7E is left, so the only whitespace is ' '.
    ascii.split(' ').filter(|w| !w.is_empty()).collect::<Vec<_>>().join(" ")
}

const ARTICLES: [&str; 9] = ["the", "a", "an", "le", "la", "les", "un", "une", "des"];

/// Search key: trimmed, lower-cased, accents folded, one leading article
/// removed, non-ASCII dropped (`normalizeString` with caseSensitive=false).
pub fn normalize_string(s: &str) -> String {
    let lowered = remove_accents(&s.trim_matches(is_js_space).to_lowercase());
    // /^(the|a|an|le|la|les|un|une|des)\s+/i
    let mut rest = lowered.as_str();
    if let Some(end) = rest.find(is_js_space) {
        let (word, tail) = rest.split_at(end);
        if ARTICLES.contains(&word) {
            rest = tail.trim_start_matches(is_js_space);
        }
    }
    rest.chars().filter(|c| c.is_ascii()).collect()
}

/// Matches the TS `ARTIST_SEPARATOR_REGEX` core (after the leading `\s*`)
/// at `chars[j..]`; returns the index just past it.
fn match_separator_core(chars: &[char], j: usize) -> Option<usize> {
    let c = *chars.get(j)?;
    if matches!(c, '/' | ';' | ',' | '&') {
        return Some(j + 1);
    }
    let boundary_before = j == 0 || !is_js_word(chars[j - 1]);
    if !boundary_before {
        return None;
    }
    let word_at = |w: &str| -> Option<usize> {
        let n = w.chars().count();
        let slice = chars.get(j..j + n)?;
        slice
            .iter()
            .zip(w.chars())
            .all(|(a, b)| a.to_ascii_lowercase() == b)
            .then_some(j + n)
    };
    let space_or_end = |k: usize| k >= chars.len() || is_js_space(chars[k]);
    // feat\.?(?=\s|$) and ft\.?(?=\s|$): try with and without the period.
    for w in ["feat", "ft"] {
        if let Some(k) = word_at(w) {
            if chars.get(k) == Some(&'.') && space_or_end(k + 1) {
                return Some(k + 1);
            }
            if space_or_end(k) {
                return Some(k);
            }
        }
    }
    // featuring\b and with\b
    for w in ["featuring", "with"] {
        if let Some(k) = word_at(w) {
            if k >= chars.len() || !is_js_word(chars[k]) {
                return Some(k);
            }
        }
    }
    None
}

/// Splits a combined artist tag ("A & B", "A feat. B", "A, B") into
/// sanitized, case-insensitively de-duplicated names.
pub fn split_artists(raw: Option<&str>) -> Vec<String> {
    let Some(raw) = raw.filter(|s| !s.is_empty()) else {
        return Vec::new();
    };
    let chars: Vec<char> = raw.chars().collect();
    let mut parts = Vec::new();
    let mut part_start = 0;
    let mut i = 0;
    while i < chars.len() {
        let mut j = i;
        while j < chars.len() && is_js_space(chars[j]) {
            j += 1;
        }
        if let Some(mut end) = match_separator_core(&chars, j) {
            while end < chars.len() && is_js_space(chars[end]) {
                end += 1;
            }
            parts.push(chars[part_start..i].iter().collect::<String>());
            part_start = end;
            i = end;
        } else {
            i += 1;
        }
    }
    parts.push(chars[part_start..].iter().collect::<String>());

    let mut seen = std::collections::HashSet::new();
    let mut result = Vec::new();
    for part in parts {
        let name = sanitize_name(&part);
        if name.is_empty() || !seen.insert(name.to_lowercase()) {
            continue;
        }
        result.push(name);
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sanitize() {
        assert_eq!(sanitize_name("  Françoise   Hardy "), "Francoise Hardy");
        assert_eq!(sanitize_name("Sigur Rós ✓"), "Sigur Ros");
        assert_eq!(sanitize_name("Œuvre"), "OEuvre");
    }

    #[test]
    fn normalize() {
        assert_eq!(normalize_string("The Beatles"), "beatles");
        assert_eq!(normalize_string("Les Négresses Vertes"), "negresses vertes");
        assert_eq!(normalize_string("An apple"), "apple");
        assert_eq!(normalize_string("Annie"), "annie");
        assert_eq!(normalize_string("The"), "the");
    }

    #[test]
    fn split() {
        let s = |x: &str| split_artists(Some(x));
        assert_eq!(s("A & B"), ["A", "B"]);
        assert_eq!(s("A feat. B"), ["A", "B"]);
        assert_eq!(s("A ft B, C"), ["A", "B", "C"]);
        assert_eq!(s("A featuring B"), ["A", "B"]);
        assert_eq!(s("A with B"), ["A", "B"]);
        assert_eq!(s("Swithin"), ["Swithin"]);
        assert_eq!(s("Feathers"), ["Feathers"]);
        assert_eq!(s("A/B;a"), ["A", "B"]);
        assert_eq!(s("A &"), ["A"]);
        assert!(split_artists(None).is_empty());
    }
}
