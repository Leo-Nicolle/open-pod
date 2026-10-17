//! Where the CLI's settings come from: command-line flags win, then the
//! OPENPOD_SD / OPENPOD_LIBRARY / OPENPOD_EXCLUDE / OPENPOD_PODCASTS
//! environment variables (as in the former sync/.env), then
//! `~/.config/opentone/config.toml` (see
//! `opentone_core::config`).

use std::path::PathBuf;

use anyhow::bail;
use opentone_core::config::{self as core_config, expand};

pub struct Config(pub core_config::Config);

fn env(key: &str) -> Option<String> {
    std::env::var(key).ok().filter(|v| !v.is_empty())
}

fn file() -> String {
    core_config::Config::path().map_or("the config file".into(), |p| p.display().to_string())
}

impl Config {
    pub fn load() -> anyhow::Result<Self> {
        Ok(Self(core_config::Config::load()?))
    }

    pub fn card(&self, flag: Option<PathBuf>) -> anyhow::Result<PathBuf> {
        match flag.or_else(|| env("OPENPOD_SD").map(PathBuf::from)).or_else(|| self.0.card.clone()) {
            Some(c) => Ok(expand(&c)),
            None => bail!("where is the card? Pass --card /mnt/opentone, set OPENPOD_SD, or set `card` in {}", file()),
        }
    }

    pub fn libraries(&self, args: Vec<PathBuf>) -> anyhow::Result<Vec<PathBuf>> {
        let libs = if !args.is_empty() {
            args
        } else if let Some(l) = env("OPENPOD_LIBRARY") {
            vec![PathBuf::from(l)]
        } else {
            self.0.libraries.clone()
        };
        if libs.is_empty() {
            bail!("which library? Pass it as an argument, set OPENPOD_LIBRARY, or set `libraries` in {}", file());
        }
        Ok(libs.iter().map(|p| expand(p)).collect())
    }

    pub fn excludes(&self, flags: Vec<String>) -> Vec<String> {
        let mut all = self.0.exclude.clone();
        if let Some(e) = env("OPENPOD_EXCLUDE") {
            all.extend(e.split(':').map(str::to_owned));
        }
        all.extend(flags);
        all
    }

    /// The podcasts folder to mirror, if any.
    pub fn podcasts(&self, flag: Option<PathBuf>) -> Option<PathBuf> {
        flag.or_else(|| env("OPENPOD_PODCASTS").map(PathBuf::from)).or_else(|| self.0.podcasts.clone()).map(|p| expand(&p))
    }

    pub fn machine(&self, flag: Option<String>) -> String {
        flag.unwrap_or_else(|| self.0.machine())
    }
}
