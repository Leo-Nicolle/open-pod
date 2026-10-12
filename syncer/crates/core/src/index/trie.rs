//! Search trie (`music_index.bin`), ported from `indexer/src/trie.ts`.
//! All keys and names are ASCII by the time they get here, so byte offsets
//! equal the JS string offsets the TS serializer computed.

use std::collections::{HashMap, VecDeque};

use super::model::CrawlIndex;
use super::text::{normalize_string, remove_accents};

const MIN_PREFIX_LENGTH: usize = 2;
const MAX_RESULTS: usize = 20;

#[derive(Clone, Copy)]
struct SearchResult {
    kind: u32, // 0=track, 1=artist, 2=album, 3=genre
    id: u32,
    name: usize, // index into Trie::names
    relevance: u32,
}

struct Node {
    key: String,
    children: Vec<usize>,
    results: Option<Vec<SearchResult>>,
}

struct Trie {
    nodes: Vec<Node>,
    names: Vec<String>,
}

fn common_prefix_len(a: &str, b: &str) -> usize {
    a.bytes().zip(b.bytes()).take_while(|(x, y)| x == y).count()
}

impl Trie {
    fn insert(&mut self, node: usize, key: &str, result: SearchResult) {
        for ci in 0..self.nodes[node].children.len() {
            let child = self.nodes[node].children[ci];
            let common = common_prefix_len(key, &self.nodes[child].key);
            if common == 0 {
                continue;
            }
            if common < self.nodes[child].key.len() {
                // Split: the child keeps the common prefix, a new node takes
                // the suffix with the old children and results.
                let suffix = self.nodes[child].key[common..].to_owned();
                let moved = Node {
                    key: suffix,
                    children: std::mem::take(&mut self.nodes[child].children),
                    results: self.nodes[child].results.take(),
                };
                self.nodes.push(moved);
                let new_id = self.nodes.len() - 1;
                self.nodes[child].key.truncate(common);
                self.nodes[child].children = vec![new_id];
            }
            let remaining = &key[common..];
            if remaining.is_empty() {
                self.nodes[child].results.get_or_insert_with(Vec::new).push(result);
                return;
            }
            self.insert(child, remaining, result);
            return;
        }
        self.nodes.push(Node { key: key.to_owned(), children: Vec::new(), results: Some(vec![result]) });
        let id = self.nodes.len() - 1;
        self.nodes[node].children.push(id);
    }

    fn add(&mut self, name: &str, kind: u32, id: u32) {
        let normalized = normalize_string(name);
        if normalized.len() < MIN_PREFIX_LENGTH {
            return;
        }
        let display: String =
            remove_accents(name).chars().filter(|c| c.is_ascii()).collect::<String>().to_lowercase();
        self.names.push(display);
        let result = SearchResult { kind, id, name: self.names.len() - 1, relevance: 100 };
        self.insert(0, &normalized, result);

        // split(/\s+/) on a trimmed string; an empty word only appears when
        // the name itself is empty, which minPrefixLength already rejected.
        for word in normalized.split(|c: char| c.is_whitespace()).filter(|w| !w.is_empty()) {
            if word.len() >= MIN_PREFIX_LENGTH && word != normalized {
                let partial = (50 - (normalized.len() as i64 - word.len() as i64)).max(25) as u32;
                let word = word.to_owned();
                self.insert(0, &word, SearchResult { relevance: partial, ..result });
            }
        }
    }

    fn sort_results(&mut self) {
        for node in &mut self.nodes {
            if let Some(results) = &mut node.results {
                results.sort_by(|a, b| b.relevance.cmp(&a.relevance)); // stable
                results.truncate(MAX_RESULTS);
            }
        }
    }
}

fn build(ix: &CrawlIndex) -> Trie {
    let mut trie = Trie {
        nodes: vec![Node { key: String::new(), children: Vec::new(), results: None }],
        names: Vec::new(),
    };
    let lists: [(&[u32], &[String], u32); 4] = [
        (&ix.track_order, &ix.tracks, 0),
        (&ix.artist_order, &ix.artists, 1),
        (&ix.album_order, &ix.albums, 2),
        (&ix.genre_order, &ix.genres, 3),
    ];
    for (order, names, kind) in lists {
        for &id in order {
            trie.add(&names[id as usize], kind, id);
        }
    }
    trie.sort_results();
    trie
}

/// Serialises to the firmware layout (see `exportToBinary` in trie.ts):
/// header {poolSize, nodeCount, resultCount, offsetCount}, string pool,
/// string offsets, nodes (6×u32), results (5×u32). Nodes are in BFS order.
pub fn export(ix: &CrawlIndex) -> Vec<u8> {
    let trie = build(ix);

    let mut bfs = Vec::with_capacity(trie.nodes.len());
    let mut queue = VecDeque::from([0usize]);
    while let Some(n) = queue.pop_front() {
        bfs.push(n);
        queue.extend(trie.nodes[n].children.iter().copied());
    }
    let mut position = vec![0u32; trie.nodes.len()];
    for (i, &n) in bfs.iter().enumerate() {
        position[n] = i as u32;
    }

    let mut pool = String::new();
    let mut offsets: Vec<u32> = Vec::new();
    let mut table: HashMap<String, u32> = HashMap::new();
    let mut intern = |s: &str, pool: &mut String| -> u32 {
        if let Some(&o) = table.get(s) {
            return o;
        }
        let o = pool.len() as u32;
        pool.push_str(s);
        offsets.push(o);
        table.insert(s.to_owned(), o);
        o
    };

    let mut nodes: Vec<[u32; 6]> = Vec::with_capacity(bfs.len());
    let mut results: Vec<[u32; 5]> = Vec::new();
    for &n in &bfs {
        let node = &trie.nodes[n];
        let key_offset = intern(&node.key, &mut pool);
        let first_result = results.len() as u32;
        let result_count = node.results.as_ref().map_or(0, Vec::len) as u32;
        for r in node.results.iter().flatten() {
            let name = &trie.names[r.name];
            let name_offset = intern(name, &mut pool);
            results.push([r.kind, r.id, name_offset, name.len() as u32, r.relevance]);
        }
        let child_count = node.children.len() as u32;
        nodes.push([
            key_offset,
            node.key.len() as u32,
            child_count,
            node.children.first().map_or(0, |&c| position[c]),
            result_count,
            if result_count > 0 { first_result } else { 0 },
        ]);
    }

    let mut out = Vec::with_capacity(16 + pool.len() + offsets.len() * 4 + nodes.len() * 24 + results.len() * 20);
    for v in [pool.len(), nodes.len(), results.len(), offsets.len()] {
        out.extend_from_slice(&(v as u32).to_le_bytes());
    }
    out.extend_from_slice(pool.as_bytes());
    for v in offsets.iter().chain(nodes.iter().flatten()).chain(results.iter().flatten()) {
        out.extend_from_slice(&v.to_le_bytes());
    }
    out
}
