# TODO

Things that came to my mind on the improvement and steps. Not ordered, not prioritised.

## UI/UX

- ~~Volume: make it use a log. only the top quarter of the volume setting is usefulll and it is too aggressive~~
- ~~Startup: add some LOGO/animations, remove the sin test, and see how we can avoid noise on the jack~~
- ~~Menus without a scrollbar: when switching to them the pixels on the scrollbar positions are not redrawn (animation side effect) and we see some artefacts of the previous menu.~~
- Settings: Add a setting window with anything that could be usefull for the user to rweak
- ~~Equilizer: see if the VS1053 has one, is it on the DAC level ? In this case we will have to sync it with the bluetooth Equilizer (if it has one)~~
- ~~Now Playing screen: it shows only the title of the track, it should show the Album and Artist as well I think. (On the title we only see the previous Menu title, and if we run it from the tracks menu for instance, we just see "Now Playing" )~~
- Goto feature: When we are on the NowPLaying we should have a way to go to the track's album/artist/genre pages, maybe by center long press or double click?
- ~~Fast scroll: Display one or two letters on top of the list to represent the current first or firsts letters in the current rows.~~
- ~~Boot: add an animtaion and sound~~
- Bluetooth: on the header bar, when no one is connected, but BT is enabled it should show with dimmed color, for now it shows the same as when connected.
- Scroll improvements: we did something already in the podcast seeking: make it go fast after a while and slower instead. I think we need for tuning for the other menus as well: it can sometimes be hard to select one entry (too sensitive it scrolls to the mext/previous one just as you lift your thumb, or sometimes you make two ticks at once while tring to reach for a track or a letter in the search). And with a large collection/list, the scrolling can feel long, we need progressive increase of speed like on the podcast timestamp seeking.
- UI and PSRAM memory allocation: we have a note about the overflowing of the index from the PSRAM, we need to handle that: measure perfs when storing the search trie and the paths/ into the SD, and also handle dynamic indexes load from the SD, by pages, cause even the reference indexes could overflow the PSRAM.
  Compute what is the best trade off between drainage, memory allocation, speed etc.

## Developement tools

- ~~ Wrap all the Serial.println into a macro and use platformio flags to turn them on/off~~
  ~~(LOG_LEVEL=0,1,2, LOG_AUDIO_BUFFER, LOG_INDEX_USAGE, LOG_USER_INPUT ... ) with some docs somewhere about all thoose flags, probably within the macro definition.COLOR ~~

## Playlists

- We should have a playlist system.
- Import them from the main playlists formats: Spotify, Tidal etc
- Have a way to add/remove stuff to playlists from the device
- Nice Random more ( a shuffle rather than a full random which would repeat.) THere is an article by spotify about it, we will look it up.

# Documentation and website

- Rework the UI builder: it scrolls way too much, make it have a screen selector, have a side panel for the theme and one button to download all the assets. Maybe annother one to save the current config in localStorage ( i think right now it saves it at any changes, which is baaaadd)
- Make it pull the software/hardware revisions from github and display it somewhere
- Include the magic Platformio button to flash OpenPod directly from the website. Would be good to be able to teak the flags (debug etc), because later we might have different flags depending on the version of the audio chip for instance (vs1063 has 5 rows equilizer, would be nice to have a settings menu for that in openpod)
- Rename the blog entries to make them clearer, maybe have some summary sentence for each ?
- Include the indexer tool directly there? So you can scan and import just by visiting the website?  

## Hardware

- Battery, buttons and power management: we are runing out of pins (and 2 are dead on the current nucleo I am using), I ll have a lock button, which might need a pin (might not if I use it to short the buttons physically?). I ll need a couple of MOSFET, maybe 3: one for the screen backlights, one for the SD card and maybe one fo the bluetooth card module ? I might be short on pins !!!!

## Indexer

- Plan: [sync-tool.md](sync-tool.md) (Rust core + CLI, additive multi-computer sync, Tauri and WASM later). Phases 1–3 are done in `syncer/` (see its README) and replace `sync/sync.sh`.
- ~~Firmware lists stop at 200 entries (and relationship lists, like an album's tracks, at 50)~~: State now loads a window of `LOOKUP_WINDOW` (64) names around the visible rows (`State::ensureWindow`, `MusicLookup` lookups take an offset), so lists have no length limit. Also fixed: the `getAll*` lookups never decremented `buffer_remaining` (overrun once names passed 8 KB), and a search hit in an album past its 50th track couldn't be played within the album.
- I might want to make it a Rust app/CMD. Tauri? I would have it compiled for all platforms on github actions and copy it to the SD card to always have it. It would be nice to have a nice managment UI for adding/sharing/removing stuff to the lib. Let's see if we need a whole collection view(Itunes like) or just a nice folder selector + loading bar.
- Could we have a emscripten version of the Rust backend and use it directly from the website? So you can add music just by visiting the website?

## Podcasts and audiobooks

 I listen quite a lot of that we would need to:

- Have a script to normalize their ID3 tags ( just like I use Beet for the music ones), is there one for podcasts ?
- We would need the indexer to crawl through them and index them
- Would be nice to have subMenus for them. The difference with the music is that the tree might not have
  the same depth everywhere.

```
      - France-Culture | - Les Pieds sur Terre
                       | - Le Cours de l'Histoire
                       | - Culture Monde
                       | - La serie Fiction  | - Le projet Horloff
                                             | - Debout les Morts
                                             | - Madame Bovery
                                             |_____________________

                       |______________________
      - L' equipe      | - L' equipe du tour
                       |______________________

```

- We should save the listenned episodes and the current timestamp within unfinished episodes. There should be some shortcut to mark an episode as read/unread from the player.
