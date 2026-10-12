# Design brief: OpenPod Sync (desktop app + web page)

## What OpenPod is

OpenPod is an open-source, DIY music player in the spirit of the iPod
Classic. It has an STM32 microcontroller, a capacitive click wheel, a small
320×240 colour screen and Bluetooth, and it plays FLAC and MP3 from a
microSD card. It has no smartphone, no cloud and no subscription: the music
lives on the SD card. The project is documented on a VitePress site (blog,
manual, tools).

## What this app does

OpenPod Sync fills the player's SD card from the music libraries on your
computers. The user plugs the SD card into a computer, picks their music
folder(s), sees what the card is missing, and copies it. A command-line tool
already does all of this. This brief is for the graphical version, which
ships as:

1. **A desktop app** (Tauri: Windows, macOS, Linux). The window is about
   1280×800 by default and must work down to 900×600.
2. **A page on the OpenPod website** (same UI, running in the browser
   through WebAssembly; Chrome/Edge only). It sits under the site's "Tools"
   section, inside the site's normal layout.

Both use the same screens, so design one UI that fits in a standalone window
and inside a documentation page.

### How sync behaves (the concepts the UI must make clear)

- **Additive.** Sync only adds or upgrades music; it never deletes. You can
  sync from several computers (for example "leo-pc" and "laptop"), each with
  a different library, and the card ends up with the union.
- **Tracks are matched by their tags** (album artist, album, disc, track
  number, title), not by file name. The same album on two computers is
  stored once. A FLAC replaces an MP3 of the same track ("upgrade").
- **Removal is a deliberate action.** Removing an album deletes it from the
  card and remembers the choice, so later syncs from any computer don't bring
  it back. A "bring removed albums back" option exists.
- **Conversion.** The player can't play everything. Hi-res FLAC (96 kHz,
  24-bit), WAV, AIFF and ALAC are converted to FLAC on the way. AAC, Ogg and
  Opus become MP3, but only if ffmpeg is installed (desktop only). Converted
  files are listed so the user knows.
- **Badly tagged files** are still copied. Their names are guessed from the
  folder layout (Artist/Album/01 Title), or they show as "Unknown", and they
  are flagged so the user can fix their tags.
- **First use with an existing card:** the app reads the whole card once
  (about one minute) to learn what is already on it. After that, a check
  takes under a second.
- **Speed is limited by the card reader.** A USB 2 reader copies at about
  20 MB/s, so a big first sync (15 GB) takes 10 to 15 minutes. Later syncs
  only copy what's new and are short. Sync can be stopped at any time and
  resumes where it left off.

## Users

- **Main user:** the person who built the player, a technical music lover
  with a carefully organised library (about 1,600 tracks, 80 albums, 330
  artists, FLAC and MP3), syncing from a desktop and a laptop.
- **Later:** other people who build an OpenPod. They are less technical and
  have less carefully tagged libraries. The UI must explain itself without
  the docs.

## Screens

Use realistic content. These are the user's real albums: *Djangologie 6
(1937)* by Django Reinhardt; *Que c'est triste Venise* by Charles Aznavour;
*J'ai de bonnes nouvelles (1977–1978)* by Anne Sylvestre; *IV* by Cypress
Hill; *Complete Decca Solo Recordings* by Radu Lupu (a 114-track box set);
*Let's Get Free* by dead prez; *Nuées ardentes* by Birds on a Wire. The card
is 119 GB, with 15 GB used.

### 1. Setup / home

- **The card:** detected automatically when plugged in. Show its name,
  capacity, free space, and how many tracks and albums are on it.
- **States:**
  - no card found (with "choose folder…");
  - card found but not readable or writable (with an explanation);
  - card never used with OpenPod (empty);
  - a card filled before this app existed (needs the one-time read).
- **Music libraries:** one or more folders, each with "add" and "remove".
  Folders to exclude are optional and live behind an "advanced" disclosure.
- **This computer's name** (for example "leo-pc"), editable, in a quiet
  spot. It shows up later as "from leo-pc".
- **Primary action:** "Check what's new".

### 2. What's new (the sync preview)

This is the most important screen. After comparing the libraries with the
card, it shows:

- **A summary:**
  - 761 tracks to add in 47 albums;
  - 3 upgrades (MP3 → FLAC);
  - 140 files will be converted;
  - 15.6 GB to write, 111 GB free;
  - estimated time, about 13 min at this card reader's speed.
- **The albums to write,** as a list or grid of cover thumbnails. Each shows
  the album, the artist, its track count, and badges for "new", "upgrade",
  "converted" and "tags guessed".
- **Skipped files,** collapsed by default: files that can't be converted
  without ffmpeg, and unreadable files.
- **"Already on the card: 823 tracks",** a calm confirmation that nothing is
  copied twice.
- **When nothing is new:** "Your card is up to date".
- **When there isn't enough space:** a clear blocking message that suggests
  removing albums.
- **Primary action:** "Sync 15.6 GB".

### 3. Syncing (progress)

- **One overall progress bar,** weighted by size, not file count.
  Copied/total (1.96 GB of 14.5 GB), real speed (18 MB/s), time elapsed and
  time left.
- **The file being written** ("Bernard Lavilliers / Les 50 plus belles
  chansons / 2-29 Faits divers.flac"). A small live list of recent albums
  completed is a nice-to-have.
- **Conversions run in parallel** with copying. Showing "converting" next to
  "copying" is optional.
- **"Stop" button:** it finishes the current file and saves; nothing is
  lost. The resulting state reads "Stopped: 85 of 761 tracks copied. Sync
  again to continue."
- **Final summary:**
  - tracks written, conversions, time taken;
  - any warnings (an album cover that couldn't be read, failed files);
  - "Eject the card before unplugging it", with an Eject button in the
    desktop app.

### 4. On the card (library view)

- **A grid of album covers** for everything on the card, with album, artist
  and track count. Sort by artist, album or date added. Search by name.
- **Album detail:**
  - the track list with durations and formats (FLAC 44.1 kHz / 16-bit,
    MP3 320k);
  - which computers it came from ("from leo-pc, laptop", or "already on the
    card" for music the app found there);
  - whether it was converted;
  - whether its tags were guessed.
- **Removal:** select one or more albums (or a whole artist), then
  "Remove from card". The confirmation dialog explains it in plain words:
  "Deletes 25 tracks (1.2 GB) from the card. Syncs from this or other
  computers won't add them back."
- **A "Removed" list,** with a "bring back" action for each item.
- **Albums with guessed or unknown tags** get a quiet badge and a filter
  ("Needs tags: 3 albums"). The fix happens outside the app, so explain
  that tags are edited in the music library and then synced again.

### 5. Report and settings (lightweight)

- **The last sync's report:** conversions made, skipped files and why,
  folders with guessed tags.
- **Settings:**
  - this computer's name;
  - the libraries and excluded folders;
  - "album grouping" (album artist + album, or album name only), which is
    advanced and rarely changed;
  - whether ffmpeg is available, with a short note on what it unlocks.
- **"Put the app on the card"** (desktop): copies the app binaries for all
  platforms onto the card, so the card carries its own sync tool to any
  computer.

## Web version differences (same screens)

- **Folder access:** the user grants access to the music folder and to the
  card through the browser's folder picker. The browser may ask again on
  each visit, so there needs to be a clear "grant access" state for both
  folders.
- **No ffmpeg in the browser:** AAC, Ogg and Opus files are skipped. Say so
  in the skipped section, and suggest the desktop app.
- **Unsupported browsers** (Firefox, Safari) get a friendly screen: "This
  needs Chrome or Edge, or download the desktop app", with download buttons.
- **Within the documentation page,** the UI sits in the content column, at
  roughly 720–960 px wide.

## Visual identity

Two existing identities can inform the design:

- **The website (VitePress):**
  - magenta `#972d6f` (light `#cb5297`, dark `#6f1d55`), green `#60a534`
    (light `#96dd42`) and cream `#e8e0cc`;
  - the hero uses a magenta-to-green gradient;
  - fonts are **Space Grotesk** for text and **IBM Plex Mono** for code and
    numbers;
  - it has light and dark modes.
- **The player's own screen:**
  - warm near-black background `#12100F`, surfaces `#4B4744`, lines
    `#968D88`, amber text `#FDA326`, accent `#EB980A` (dark `#664014`,
    light `#FFD15C`), destructive `#D9665C`;
  - it is iPod-like: lists, a header bar, album covers and a progress bar.

A good direction ties the two together: the website's colours and fonts for
the app's chrome, with a nod to the player (album-cover-forward, list
rhythm, warm dark mode). Design **both light and dark themes.** Numbers
(sizes, speeds, counts, durations) are central to this app, so use tabular
or monospace digits for them.

## Tone

- **Calm and reassuring:** the user is handing their music to a tool that
  writes to a card. Say clearly what will happen before it happens. Never
  make deleting easy by accident.
- **Plain words:** "Card", "Music folder", "Add", "Remove from card",
  "Converted for the player". Avoid "manifest", "adopt", "tombstone",
  "LPC order" and "sync source".
- **Honest about time:** the card reader is slow, so show real speed and
  time left, and explain why when it's slow ("USB 2 card reader: about
  20 MB/s").

## Deliverables

- **Desktop window at 1280×800:** Setup (with the card detected and with no
  card), What's new, Syncing, Sync finished, On the card (grid), Album
  detail, the Remove confirmation, and Settings.
- **The narrow and web variants:** What's new and Syncing at 900 px, and
  inside a documentation page at about 760 px.
- **Light and dark themes** for at least What's new and On the card.
- **Key states:**
  - the first-time card read, with a progress bar;
  - card up to date;
  - not enough space;
  - stopped mid-sync;
  - unsupported browser;
  - the web page's "grant folder access" state.
- **A small component sheet:** buttons, badges (new, upgrade, converted,
  tags guessed, removed), the album card, the progress bar, and the
  confirmation dialog.
