//! Finding SD cards among the mounted volumes, and ejecting them.

use std::path::{Path, PathBuf};
use std::process::Command;

use openpod_core::manifest;

/// A mounted volume that could be an OpenPod card.
#[derive(Debug, Clone)]
pub struct Volume {
    pub mount: PathBuf,
    pub label: Option<String>,
    /// Block device (Linux/macOS), for eject.
    pub device: String,
}

fn is_fat(fs: &str) -> bool {
    let fs = fs.to_ascii_lowercase();
    ["vfat", "fat", "fat32", "msdos", "exfat"].iter().any(|f| fs == *f)
}

/// Volume labels by device on Linux (`/dev/disk/by-label/<label>` links).
#[cfg(target_os = "linux")]
fn linux_label(device: &str) -> Option<String> {
    let dev = std::fs::canonicalize(device).ok()?;
    for e in std::fs::read_dir("/dev/disk/by-label").ok()?.flatten() {
        if std::fs::canonicalize(e.path()).ok().as_deref() == Some(dev.as_path()) {
            // udev escapes spaces and other bytes as \xNN.
            let raw = e.file_name().to_string_lossy().into_owned();
            let mut out = String::new();
            let mut rest = raw.as_str();
            while let Some(i) = rest.find("\\x") {
                out.push_str(&rest[..i]);
                match u8::from_str_radix(rest.get(i + 2..i + 4).unwrap_or(""), 16) {
                    Ok(b) => {
                        out.push(b as char);
                        rest = &rest[i + 4..];
                    }
                    Err(_) => {
                        out.push_str("\\x");
                        rest = &rest[i + 2..];
                    }
                }
            }
            out.push_str(rest);
            return Some(out);
        }
    }
    None
}

/// Removable FAT/exFAT volumes, and any volume that already holds an
/// OpenPod library.
pub fn detect() -> Vec<Volume> {
    let disks = sysinfo::Disks::new_with_refreshed_list();
    let mut out: Vec<Volume> = Vec::new();
    for d in disks.list() {
        let mount = d.mount_point().to_path_buf();
        let fs = d.file_system().to_string_lossy();
        let openpod = manifest::exists(&mount);
        if !(openpod || (d.is_removable() && is_fat(&fs))) {
            continue;
        }
        if out.iter().any(|v| v.mount == mount) {
            continue;
        }
        let device = d.name().to_string_lossy().into_owned();
        #[cfg(target_os = "linux")]
        let label = linux_label(&device);
        #[cfg(not(target_os = "linux"))]
        let label = Some(device.clone()).filter(|n| !n.is_empty());
        out.push(Volume { mount, label, device });
    }
    out
}

/// The volume mounted at `path`, if any.
pub fn volume_at(path: &Path) -> Option<Volume> {
    let disks = sysinfo::Disks::new_with_refreshed_list();
    let d = disks.list().iter().find(|d| d.mount_point() == path)?;
    let device = d.name().to_string_lossy().into_owned();
    #[cfg(target_os = "linux")]
    let label = linux_label(&device);
    #[cfg(not(target_os = "linux"))]
    let label = Some(device.clone()).filter(|n| !n.is_empty());
    Some(Volume { mount: path.to_path_buf(), label, device })
}

fn run(cmd: &mut Command) -> Result<(), String> {
    let out = cmd.output().map_err(|e| e.to_string())?;
    if out.status.success() {
        Ok(())
    } else {
        Err(String::from_utf8_lossy(&out.stderr).trim().to_owned())
    }
}

/// Unmounts the card and, where the OS allows, powers the reader slot off.
pub fn eject(mount: &Path) -> Result<(), String> {
    let vol = volume_at(mount);
    #[cfg(target_os = "linux")]
    {
        let dev = vol.map(|v| v.device).unwrap_or_default();
        if !dev.is_empty() && run(Command::new("udisksctl").args(["unmount", "-b", &dev])).is_ok() {
            let _ = run(Command::new("udisksctl").args(["power-off", "-b", &dev]));
            return Ok(());
        }
        run(Command::new("umount").arg(mount)).map_err(|e| {
            format!("{e}. Unmount the card from your file manager (or `sudo umount {}`).", mount.display())
        })
    }
    #[cfg(target_os = "macos")]
    {
        let _ = vol;
        run(Command::new("diskutil").arg("eject").arg(mount))
    }
    #[cfg(target_os = "windows")]
    {
        let _ = vol;
        let drive = mount.to_string_lossy().trim_end_matches('\\').to_owned();
        let script =
            format!("(New-Object -comObject Shell.Application).Namespace(17).ParseName('{drive}').InvokeVerb('Eject')");
        run(Command::new("powershell").args(["-NoProfile", "-Command", &script]))
    }
}
