//! `~/.config/openpod/config.toml` (all keys optional):
//!
//! ```toml
//! machine = "laptop"          # name in the card's manifest (default: hostname)
//! card = "/mnt/openpod"       # mounted SD card
//! libraries = ["~/Music"]
//! exclude = ["Podcasts", "Various Artists/Bootlegs"]  # library-relative
//! album_key = "artist-album"  # or "name"
//! ```
//!
//! For the CLI, flags win, then the OPENPOD_SD / OPENPOD_LIBRARY /
//! OPENPOD_EXCLUDE environment variables (as in the former sync/.env), then
//! this file. The app reads and edits the file itself.

use std::io;
use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};

use crate::index::AlbumKey;

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Config {
    pub machine: Option<String>,
    pub card: Option<PathBuf>,
    #[serde(default)]
    pub libraries: Vec<PathBuf>,
    #[serde(default)]
    pub exclude: Vec<String>,
    pub album_key: Option<AlbumKey>,
}

/// `~/x` → `$HOME/x`.
pub fn expand(p: &Path) -> PathBuf {
    match (p.strip_prefix("~"), dirs::home_dir()) {
        (Ok(rest), Some(home)) => home.join(rest),
        _ => p.to_path_buf(),
    }
}

/// `$HOME/x` → `~/x`, the way paths are written in the file.
fn contract(p: &Path) -> String {
    match dirs::home_dir().and_then(|h| p.strip_prefix(h).ok().map(Path::to_path_buf)) {
        Some(rest) if !rest.as_os_str().is_empty() => format!("~/{}", rest.to_string_lossy()),
        _ => p.to_string_lossy().into_owned(),
    }
}

pub fn hostname() -> String {
    gethostname::gethostname().to_string_lossy().into_owned()
}

impl Config {
    pub fn path() -> Option<PathBuf> {
        dirs::config_dir().map(|d| d.join("openpod").join("config.toml"))
    }

    pub fn load() -> io::Result<Self> {
        let Some(path) = Self::path().filter(|p| p.is_file()) else { return Ok(Self::default()) };
        let text = std::fs::read_to_string(&path)?;
        toml::from_str(&text)
            .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, format!("{}: {e}", path.display())))
    }

    /// Writes the file, keeping its comments and layout (only the keys
    /// change).
    pub fn save(&self) -> io::Result<()> {
        let path = Self::path().ok_or_else(|| io::Error::other("no config directory"))?;
        let text = std::fs::read_to_string(&path).unwrap_or_default();
        let mut doc: toml_edit::DocumentMut = text.parse().unwrap_or_default();
        let opt = |doc: &mut toml_edit::DocumentMut, key: &str, v: Option<String>| match v {
            Some(v) => doc[key] = toml_edit::value(v),
            None => {
                doc.remove(key);
            }
        };
        let list = |doc: &mut toml_edit::DocumentMut, key: &str, v: Vec<String>| {
            if v.is_empty() {
                doc.remove(key);
            } else {
                doc[key] = toml_edit::value(v.into_iter().collect::<toml_edit::Array>());
            }
        };
        opt(&mut doc, "machine", self.machine.clone());
        opt(&mut doc, "card", self.card.as_deref().map(contract));
        list(&mut doc, "libraries", self.libraries.iter().map(|p| contract(p)).collect());
        list(&mut doc, "exclude", self.exclude.clone());
        opt(
            &mut doc,
            "album_key",
            self.album_key.map(|k| match k {
                AlbumKey::Name => "name".into(),
                AlbumKey::ArtistAlbum => "artist-album".into(),
            }),
        );
        std::fs::create_dir_all(path.parent().unwrap())?;
        std::fs::write(path, doc.to_string())
    }

    /// The computer's name in card manifests: configured, else the hostname.
    pub fn machine(&self) -> String {
        self.machine.clone().unwrap_or_else(hostname)
    }

    pub fn libraries(&self) -> Vec<PathBuf> {
        self.libraries.iter().map(|p| expand(p)).collect()
    }

    pub fn card(&self) -> Option<PathBuf> {
        self.card.as_deref().map(expand)
    }

    pub fn album_key(&self) -> AlbumKey {
        self.album_key.unwrap_or_default()
    }
}
