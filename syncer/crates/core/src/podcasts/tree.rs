//! The podcast browser's tables (`openpod/podcasts.pack`): a folder tree of
//! any depth (publisher / show / [series] / episode) mirrored from the
//! podcasts folder.
//!
//! Ids follow a depth-first walk in display order: folder 0 is the root,
//! subfolders are alphabetical, a folder's own episodes come before its
//! subfolders' and are sorted by number (a series) or newest first (dated).
//! So every relation's targets, ascending, are already in display order, and
//! the episodes below a folder form one id range (`podcast_folder_span`):
//! the firmware rolls a folder's state up with one linear scan.

use std::collections::BTreeMap;

use crate::index::binary::{dense_u32, durations, relation, string_index};
use crate::index::pack::section;
use crate::index::model::Relation;
use crate::index::text::sanitize_name;

/// `podcast_folder_parent` of the root.
pub const NO_PARENT: u32 = u32::MAX;
/// `podcast_folder_flags` bit: every episode title starts with "N/M".
pub const SERIES: u32 = 1;

/// One episode, as the tree needs it.
#[derive(Debug, Clone)]
pub struct EpisodeIn<'a> {
    /// Folder names from the podcasts root down (raw).
    pub folders: &'a [String],
    pub title: &'a str,
    /// Days since 1970-01-01, 0 when unknown.
    pub date: u32,
    pub duration: f64,
    /// Card path.
    pub path: &'a str,
}

#[derive(Debug, Clone)]
pub struct Folder {
    /// Display name (ASCII, for the firmware fonts).
    pub name: String,
    pub parent: u32,
    pub series: bool,
    /// Card directory, '/'-separated ("Podcasts/France Culture").
    pub dir: String,
    /// Its own episodes' ids, in display order.
    pub episodes: Vec<u32>,
    pub children: Vec<u32>,
    /// Episode ids below it (own and in subfolders): [first, end).
    pub span: (u32, u32),
}

#[derive(Debug, Default)]
pub struct Tree {
    pub folders: Vec<Folder>,
    /// Episode id -> index in the input.
    pub order: Vec<usize>,
    pub titles: Vec<String>,
    /// `n | total << 16`, 0 when the title has no "N/M" prefix.
    pub numbers: Vec<u32>,
}

/// "1/10 : Le hêtre" -> (1, 10, "Le hêtre"). The separator after the
/// number (" : ", ":", " ") goes with it.
pub fn split_number(title: &str) -> Option<(u32, u32, &str)> {
    fn digits(s: &str) -> Option<(u32, &str)> {
        let end = s.find(|c: char| !c.is_ascii_digit()).unwrap_or(s.len());
        if end == 0 || end > 4 {
            return None;
        }
        Some((s[..end].parse().ok()?, &s[end..]))
    }
    let (n, rest) = digits(title.trim_start())?;
    let rest = rest.trim_start().strip_prefix('/')?;
    let (total, rest) = digits(rest.trim_start())?;
    let rest = rest.trim_start();
    let rest = rest.strip_prefix(':').unwrap_or(rest).trim_start();
    Some((n, total, rest))
}

#[derive(Default)]
struct Node {
    children: BTreeMap<String, Node>,
    episodes: Vec<usize>,
}

pub fn build(eps: &[EpisodeIn]) -> Tree {
    let mut root = Node::default();
    for (i, e) in eps.iter().enumerate() {
        let mut node = &mut root;
        for f in e.folders {
            node = node.children.entry(f.clone()).or_default();
        }
        node.episodes.push(i);
    }
    let coll = crate::index::model::collator();
    let mut tree = Tree::default();
    walk(&root, "Podcasts".into(), NO_PARENT, "Podcasts", eps, &coll, &mut tree);
    tree
}

fn walk(
    node: &Node,
    name: String,
    parent: u32,
    dir: &str,
    eps: &[EpisodeIn],
    coll: &icu_collator::CollatorBorrowed,
    tree: &mut Tree,
) -> u32 {
    let id = tree.folders.len() as u32;
    let first = tree.order.len() as u32;
    let mut own: Vec<(usize, Option<(u32, u32, &str)>)> =
        node.episodes.iter().map(|&i| (i, split_number(eps[i].title))).collect();
    let series = !own.is_empty() && own.iter().all(|(_, n)| n.is_some());
    if series {
        own.sort_by(|a, b| {
            let (an, bn) = (a.1.unwrap(), b.1.unwrap());
            (an.0, an.2).cmp(&(bn.0, bn.2))
        });
    } else {
        own.sort_by(|a, b| eps[b.0].date.cmp(&eps[a.0].date).then_with(|| eps[a.0].path.cmp(eps[b.0].path)));
    }
    let mut episodes = Vec::with_capacity(own.len());
    for (i, n) in own {
        episodes.push(tree.order.len() as u32);
        tree.order.push(i);
        let (title, number) = match n.filter(|_| series) {
            Some((n, total, rest)) => (rest, n.min(0xFFFF) | total.min(0xFFFF) << 16),
            None => (eps[i].title, 0),
        };
        tree.titles.push(sanitize_name(title));
        tree.numbers.push(number);
    }
    tree.folders.push(Folder {
        name: sanitize_name(&name),
        parent,
        series,
        dir: dir.to_owned(),
        episodes,
        children: Vec::new(),
        span: (first, first),
    });

    let mut kids: Vec<(&String, &Node)> = node.children.iter().collect();
    kids.sort_by(|a, b| coll.compare(&sanitize_name(a.0), &sanitize_name(b.0)));
    for (kid_name, kid) in kids {
        // The card folder: the first episode below it has the sanitized name.
        let kid_dir = first_episode(kid)
            .and_then(|i| {
                let depth = dir.matches('/').count() + 2;
                let parts: Vec<&str> = eps[i].path.split('/').collect();
                (parts.len() > depth).then(|| parts[..depth].join("/"))
            })
            .unwrap_or_else(|| format!("{dir}/{kid_name}"));
        let kid_id = walk(kid, kid_name.clone(), id, &kid_dir, eps, coll, tree);
        tree.folders[id as usize].children.push(kid_id);
    }
    tree.folders[id as usize].span.1 = tree.order.len() as u32;
    id
}

fn first_episode(node: &Node) -> Option<usize> {
    node.episodes.first().copied().or_else(|| node.children.values().find_map(first_episode))
}

impl Tree {
    /// The tables, as (file name, bytes). Covers and state are written by
    /// the caller.
    /// The tables as `podcasts.pack` sections (the cover index is added by
    /// the caller).
    pub fn sections(&self, eps: &[EpisodeIn]) -> Vec<(u32, Vec<u8>)> {
        let f = &self.folders;
        let rel = |pick: &dyn Fn(&Folder) -> &Vec<u32>| -> Relation {
            f.iter().enumerate().map(|(i, x)| (i as u32, pick(x).iter().copied().collect())).collect()
        };
        let ep = |pick: &dyn Fn(&EpisodeIn) -> u32| -> Vec<u32> { self.order.iter().map(|&i| pick(&eps[i])).collect() };
        let mut episode_folder = vec![0u32; self.order.len()];
        for (id, x) in f.iter().enumerate() {
            for &e in &x.episodes {
                episode_folder[e as usize] = id as u32;
            }
        }
        let names: Vec<String> = f.iter().map(|x| x.name.clone()).collect();
        let paths: Vec<String> = self.order.iter().map(|&i| eps[i].path.to_owned()).collect();
        let secs: Vec<f64> = self.order.iter().map(|&i| eps[i].duration).collect();
        let spans: Vec<u32> = f.iter().flat_map(|x| [x.span.0, x.span.1]).collect();
        vec![
            (section::POD_FOLDER_NAMES, string_index(&names)),
            (section::POD_FOLDER_PARENT, dense_u32(&f.iter().map(|x| x.parent).collect::<Vec<_>>())),
            (section::POD_FOLDER_FLAGS, dense_u32(&f.iter().map(|x| if x.series { SERIES } else { 0 }).collect::<Vec<_>>())),
            (section::POD_FOLDER_SPAN, dense_u32(&spans)),
            (section::POD_FOLDER_CHILDREN, relation(&rel(&|x| &x.children), f.len())),
            (section::POD_FOLDER_EPISODES, relation(&rel(&|x| &x.episodes), f.len())),
            (section::POD_EPISODE_TITLES, string_index(&self.titles)),
            (section::POD_EPISODE_PATHS, string_index(&paths)),
            (section::POD_EPISODE_DURATIONS, durations(&secs)),
            (section::POD_EPISODE_FOLDER, dense_u32(&episode_folder)),
            (section::POD_EPISODE_DATE, dense_u32(&ep(&|e| e.date))),
            (section::POD_EPISODE_NUMBER, dense_u32(&self.numbers)),
        ]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ep<'a>(folders: &'a [String], title: &'a str, date: u32, path: &'a str) -> EpisodeIn<'a> {
        EpisodeIn { folders, title, date, duration: 60.0, path }
    }

    #[test]
    fn numbers() {
        assert_eq!(split_number("1/10 : Le hêtre"), Some((1, 10, "Le hêtre")));
        assert_eq!(split_number(" 2 / 10:Debout"), Some((2, 10, "Debout")));
        assert_eq!(split_number("3/4 Titre"), Some((3, 4, "Titre")));
        assert_eq!(split_number("2026/10 n'est pas"), Some((2026, 10, "n'est pas")));
        assert_eq!(split_number("Debout les morts 10/10"), None);
        assert_eq!(split_number("1 : x"), None);
    }

    #[test]
    fn layout() {
        let fc = vec!["France Culture".to_string(), "Chemins".to_string()];
        let fv = vec!["France Culture".to_string(), "Fiction".to_string(), "Vargas".to_string()];
        let eps = vec![
            ep(&fc, "Older", 100, "Podcasts/France Culture/Chemins/a.mp3"),
            ep(&fv, "2/2 : Fin", 0, "Podcasts/France Culture/Fiction/Vargas/2.mp3"),
            ep(&fc, "Newer", 200, "Podcasts/France Culture/Chemins/b.mp3"),
            ep(&fv, "1/2 : Début", 0, "Podcasts/France Culture/Fiction/Vargas/1.mp3"),
            ep(&[], "Loose", 5, "Podcasts/loose.mp3"),
        ];
        let t = build(&eps);
        let names: Vec<&str> = t.folders.iter().map(|f| f.name.as_str()).collect();
        assert_eq!(names, ["Podcasts", "France Culture", "Chemins", "Fiction", "Vargas"]);
        // Root's own episode first, then depth-first: Chemins newest first,
        // Vargas by number.
        assert_eq!(t.order, [4, 2, 0, 3, 1]);
        assert_eq!(t.titles, ["Loose", "Newer", "Older", "Debut", "Fin"]);
        assert_eq!(t.numbers[3], 1 | 2 << 16);
        assert!(t.folders[4].series && !t.folders[2].series);
        assert_eq!(t.folders[0].span, (0, 5));
        assert_eq!(t.folders[1].span, (1, 5));
        assert_eq!(t.folders[3].span, (3, 5));
        assert_eq!(t.folders[3].dir, "Podcasts/France Culture/Fiction");
        assert_eq!(t.folders[1].children, [2, 3]);
        assert_eq!(t.folders[4].parent, 3);
        assert_eq!(t.sections(&eps).len(), 12);
    }
}
