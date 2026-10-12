//! `openpod/podcast_state.bin`: each episode's listening state, one
//! fixed-size record per episode id so the firmware reads and writes it in
//! place. Episode ids change on every rebuild, so each record carries the
//! episode's content key and [`remap`] moves the records to the new ids.
//!
//! ```text
//! header (16 B): "OPPS" | u16 version | u16 record size | u32 count | u32 generation
//! record (20 B): u64 key | u32 position_s | u32 last_played | u32 flags
//! ```
//!
//! `generation` must equal `podcast_meta.bin`'s: on a mismatch (a sync
//! interrupted between the two files) the firmware leaves the file alone.
//! `last_played` is a sequence number the firmware bumps (no clock needed):
//! the highest in-progress one is the "continue" row.

pub const FILE: &str = "podcast_state.bin";
pub const MAGIC: [u8; 4] = *b"OPPS";
pub const VERSION: u16 = 1;
pub const HEADER: usize = 16;
pub const RECORD: usize = 20;
/// `flags` bit: listened to the end, or marked as played.
pub const PLAYED: u32 = 1;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Record {
    pub key: u64,
    pub position: u32,
    pub last_played: u32,
    pub flags: u32,
}

impl Record {
    /// Nothing to remember: a new episode.
    pub fn is_blank(&self) -> bool {
        self.position == 0 && self.last_played == 0 && self.flags == 0
    }
}

fn u16_at(b: &[u8], at: usize) -> u16 {
    u16::from_le_bytes([b[at], b[at + 1]])
}

fn u32_at(b: &[u8], at: usize) -> u32 {
    u32::from_le_bytes(b[at..at + 4].try_into().unwrap())
}

/// (generation, records), or None when the bytes aren't a state file this
/// version understands.
pub fn parse(b: &[u8]) -> Option<(u32, Vec<Record>)> {
    if b.len() < HEADER || b[..4] != MAGIC || u16_at(b, 4) != VERSION {
        return None;
    }
    let size = u16_at(b, 6) as usize;
    let count = u32_at(b, 8) as usize;
    if size < RECORD || b.len() < HEADER + count.checked_mul(size)? {
        return None;
    }
    let records = (0..count)
        .map(|i| {
            let at = HEADER + i * size;
            Record {
                key: u64::from_le_bytes(b[at..at + 8].try_into().unwrap()),
                position: u32_at(b, at + 8),
                last_played: u32_at(b, at + 12),
                flags: u32_at(b, at + 16),
            }
        })
        .collect();
    Some((u32_at(b, 12), records))
}

pub fn encode(generation: u32, records: &[Record]) -> Vec<u8> {
    let mut out = Vec::with_capacity(HEADER + records.len() * RECORD);
    out.extend_from_slice(&MAGIC);
    out.extend_from_slice(&VERSION.to_le_bytes());
    out.extend_from_slice(&(RECORD as u16).to_le_bytes());
    out.extend_from_slice(&(records.len() as u32).to_le_bytes());
    out.extend_from_slice(&generation.to_le_bytes());
    for r in records {
        out.extend_from_slice(&r.key.to_le_bytes());
        out.extend_from_slice(&r.position.to_le_bytes());
        out.extend_from_slice(&r.last_played.to_le_bytes());
        out.extend_from_slice(&r.flags.to_le_bytes());
    }
    out
}

/// Marks `key`'s episode played or not, in place, as the player's "Mark as
/// played/unplayed" does (unplayed also rewinds to the start). False when
/// `b` isn't a state file or has no record for it.
pub fn set_played(b: &mut [u8], key: u64, played: bool) -> bool {
    let Some((_, records)) = parse(b) else { return false };
    let Some(i) = records.iter().position(|r| r.key == key) else { return false };
    let at = HEADER + i * u16_at(b, 6) as usize;
    let mut flags = records[i].flags;
    if played {
        flags |= PLAYED;
    } else {
        flags &= !PLAYED;
        b[at + 8..at + 12].copy_from_slice(&0u32.to_le_bytes());
    }
    b[at + 16..at + 20].copy_from_slice(&flags.to_le_bytes());
    true
}

/// One record per key, in `keys` order: the old record of the same episode
/// when there is one, else a blank one. Episodes gone from `keys` are
/// dropped.
pub fn remap(old: &[Record], keys: &[u64]) -> Vec<Record> {
    let by_key: std::collections::HashMap<u64, &Record> = old.iter().map(|r| (r.key, r)).collect();
    keys.iter()
        .map(|&key| by_key.get(&key).map_or(Record { key, ..Default::default() }, |r| Record { key, ..**r }))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn roundtrip() {
        let rs = vec![
            Record { key: 1, position: 300, last_played: 4, flags: 0 },
            Record { key: u64::MAX, position: 0, last_played: 0, flags: PLAYED },
        ];
        let b = encode(7, &rs);
        assert_eq!(b.len(), HEADER + 2 * RECORD);
        assert_eq!(parse(&b), Some((7, rs)));
    }

    #[test]
    fn set_played_in_place() {
        let rs = [
            Record { key: 1, position: 300, last_played: 4, flags: 0 },
            Record { key: 2, position: 900, last_played: 5, flags: PLAYED },
        ];
        let mut b = encode(7, &rs);
        assert!(set_played(&mut b, 1, true));
        assert!(set_played(&mut b, 2, false));
        assert!(!set_played(&mut b, 3, true));
        assert_eq!(
            parse(&b),
            Some((
                7,
                vec![
                    Record { key: 1, position: 300, last_played: 4, flags: PLAYED },
                    Record { key: 2, position: 0, last_played: 5, flags: 0 },
                ]
            ))
        );
        assert!(!set_played(&mut b"nope".to_vec(), 1, true));
    }

    #[test]
    fn rejects_garbage() {
        assert_eq!(parse(b"nope"), None);
        let mut b = encode(1, &[Record::default()]);
        b.truncate(b.len() - 1);
        assert_eq!(parse(&b), None);
    }

    #[test]
    fn remap_follows_keys() {
        let old = vec![
            Record { key: 10, position: 60, last_played: 2, flags: 0 },
            Record { key: 20, position: 0, last_played: 1, flags: PLAYED },
            Record { key: 30, position: 5, last_played: 3, flags: 0 },
        ];
        // 30 removed, 40 new, 10 and 20 swapped places.
        let new = remap(&old, &[20, 40, 10]);
        assert_eq!(new[0], old[1]);
        assert_eq!(new[1], Record { key: 40, ..Default::default() });
        assert_eq!(new[2], old[0]);
    }
}
