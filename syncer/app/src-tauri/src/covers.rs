//! `cover://localhost/<absolute track path>`: a small JPEG of the album
//! art (embedded, else the folder's cover image), cached in memory.

use std::collections::HashMap;
use std::io::Cursor;
use std::path::Path;
use std::sync::{Arc, Mutex};

use image::imageops::FilterType;
use openpod_core::index::crawl;
use tauri::http::{Response, StatusCode, header};

/// Thumbnail edge in pixels: album cards are 148–220 px wide, at 2x.
const SIZE: u32 = 320;

#[derive(Default)]
pub struct Cache(Mutex<HashMap<String, Option<Arc<Vec<u8>>>>>);

fn percent_decode(s: &str) -> String {
    let b = s.as_bytes();
    let mut out = Vec::with_capacity(b.len());
    let mut i = 0;
    while i < b.len() {
        if b[i] == b'%' && i + 2 < b.len() {
            if let Some(v) = std::str::from_utf8(&b[i + 1..i + 3]).ok().and_then(|h| u8::from_str_radix(h, 16).ok()) {
                out.push(v);
                i += 3;
                continue;
            }
        }
        out.push(b[i]);
        i += 1;
    }
    String::from_utf8_lossy(&out).into_owned()
}

fn thumbnail(track: &Path) -> Option<Vec<u8>> {
    let art = crawl::album_art_source(track)?;
    let img = image::load_from_memory(&art).ok()?;
    let img = img.resize_to_fill(SIZE, SIZE, FilterType::Triangle).to_rgb8();
    let mut out = Cursor::new(Vec::new());
    image::codecs::jpeg::JpegEncoder::new_with_quality(&mut out, 85).encode_image(&img).ok()?;
    Some(out.into_inner())
}

/// `uri_path` is the request path: "/" + the percent-encoded file path.
pub fn respond(cache: &Cache, uri_path: &str) -> Response<Vec<u8>> {
    let path = percent_decode(uri_path.strip_prefix('/').unwrap_or(uri_path));
    let cached = cache.0.lock().unwrap().get(&path).cloned();
    let jpeg = match cached {
        Some(j) => j,
        None => {
            let j = thumbnail(Path::new(&path)).map(Arc::new);
            cache.0.lock().unwrap().insert(path, j.clone());
            j
        }
    };
    match jpeg {
        Some(j) => Response::builder()
            .header(header::CONTENT_TYPE, "image/jpeg")
            .header(header::CACHE_CONTROL, "max-age=3600")
            .body(j.to_vec())
            .unwrap(),
        None => Response::builder().status(StatusCode::NOT_FOUND).body(Vec::new()).unwrap(),
    }
}
