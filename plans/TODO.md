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
- ~~UI and PSRAM memory allocation: we have a note about the overflowing of the index from the PSRAM, we need to handle that: measure perfs when storing the search trie and the paths/ into the SD, and also handle dynamic indexes load from the SD, by pages, cause even the reference indexes could overflow the PSRAM.~~
- Compute what is the best trade off between drainage, memory allocation, speed etc.

### Bugs

- Did we lost the covers display in the NowPlaying? They show only during the transition back to the tracks list.
- Did we lost the auto play when selecting a track as well? => center button selects the track without starting it, play button selects and starts playing. Both should have the same behaviour(select and start)
- On the Now playing screen, pushing the bottom (play/pause) button enables the seek mode. Why not, but then it should ignore the first few scroll events, otherwise we end up navigating in the track while we only wanted to pause/play. Maybe seek mode should only be enalbed from center press?
- Rounded bars (volume, track) are wrong: as if the center of the circle(rounded end) was too small (top corners are not as round as bottom, and it does not describe one single curve.)
- ~~And the `after some time gets back to now playing, then blank screen` too?~~

## Hardware

- Measure the current drain of each part (mpr121, SD, vs1053, bluetooth, screen and MCU) and see how much battery we would need.
- See if we would benefit from a high res DAC (Cirrus Logic CS43131), how much pain/gain it would give: space, drain, output capabilities (headphones, hifi  etc), would I even hear the diff?

### Clickwheel

- See if we can live without the central button being capacitive (simpler), I noticed that the thumb switching form the wheel to the central button could trigger a scroll which is anoying, I thought that disabling the scroll if any touch is detected on the center button would fix it. But maybe can be done from the button shape(higher) and software.
- Check the size and design of the current one in KiCAD, not sure it fits the ipod classic case perfectly. (print a quick 3d shim and test)
- Design the articulation between the center and the outer ring, and check that it does not escape from its slot in the case

### Settings

- Have settings for quick actions on the buttons (long press or double/triple press): back-to-now-playing, light/dark theme switch, mark as read(podcast), switch off
- Have settings for the next/prev track behaviour: gapless or with gap. Need to experiment proper track seeking and switching: we were hearing glitches when doing it but I realise it might be because we were feeding the buffer with annother chunk of data without properlly terminating the current one? Does MP3/FLAC have some minimum block resolution for decoding?

### Testing/Reporting tools

- ~~Would be good to have a page on the website to see the perfs: with visualisation of the screen framerate, audio buffers filling status, memory heap etc.~~
- ~~Would be good to implement a test bench:~~
  - ~~tranfert to the card an opentone/ folder with a biiiig dataset, no need for the audio files~~
  - ~~transfert one sample file of each audio quality: from small mp3 to large flac.~~
  - ~~make it boot, and send commands via serial: scroll, menu change, audio playback, seek, next, prev, search in the lib. ~~
  - ~~monitor the perfs, reccord a report for each version.~~
  - ~~publish it on the docs.~~
  - ~~Benchmark: add some fast scrolling there, I dont think I have seen it yet~~
  - ~~Benchmark: In the search: check that we dont input letters too fast: if we are typing faster than the trie is searching, it defeats the purpose because the longer the input the faster the search ? (at least the loading of the results I guess)~~
  - Benchmark: The tracks do not have album arts, they should
- ~~Wrap all the Serial.println into a macro and use platformio flags to turn them on/off~~
  ~~(LOG_LEVEL=0,1,2, LOG_AUDIO_BUFFER, LOG_INDEX_USAGE, LOG_USER_INPUT ... ) with some docs somewhere about all thoose flags, probably within the macro definition.COLOR ~~
- Benchmark: the compare tool fails to report to `worse` entries: it sets them to 0 while there are actually plenty
- The changes on the more tight memory alocation seemed to have broken the audio feed: it repports a lot of runway, does the buffer gets starved ? Maybe we have been too aggressive.

## Playlists

- We should have a playlist system.
- Import them from the main playlists formats: Spotify, Tidal etc
- Have a way to add/remove stuff to playlists from the device
- Nice Random more ( a shuffle rather than a full random which would repeat.) THere is an article by spotify about it, we will look it up.

# Documentation and website

- ~~Rework the UI builder: it scrolls way too much, make it have a screen selector, have a side panel for the theme and one button to download all the assets. Maybe annother one to save the current config in localStorage ( i think right now it saves it at any changes, which is baaaadd)~~
- ~~Make it pull the software/hardware revisions from github and display it somewhere~~
- Include the magic Platformio button to flash opentone directly from the website. Would be good to be able to teak the flags (debug etc), because later we might have different flags depending on the version of the audio chip for instance (vs1063 has 5 rows equilizer, would be nice to have a settings menu for that in opentone)
- ~~Rename the blog entries to make them clearer, maybe have some summary sentence for each ?~~
- ~~Include the indexer tool directly there? So you can scan and import just by visiting the website? ~~

## Naming

- ~~ Is it bad to be called opentone, and to be a music player board, that fits exaclty into an official (aliexpress) Apple Ipod Case ? Can I get in trouble ? How can I rename this ?~~

## Timeline

- I would looove to have a timeline pagee on the docs, showing the evolution of the project since the begining, referencing the key points, and maybe the blog posts. A chart with the amount of commits, or the SLOC, over time would be great. An also maybe a chart with multiple stacked lines/areas for features, hardware, etc in percentage (magic hat stats).
- I would also like to have some projection timeline, with deadlines, would help finish this project, instead of putting it back in 2 weeks for a year again in the drawer.

## Hardware

- Battery, buttons and power management: we are runing out of pins (and 2 are dead on the current nucleo I am using), I ll have a lock button, which might need a pin (might not if I use it to short the buttons physically?). I ll need a couple of MOSFET, maybe 3: one for the screen backlights, one for the SD card and maybe one fo the bluetooth card module ? I might be short on pins !!!!

## Indexer

-~~Plan: [sync-tool.md](sync-tool.md) (Rust core + CLI, additive multi-computer sync, Tauri and WASM later). Phases 1–3 are done in `syncer/` (see its README) and replace `sync/sync.sh`.~~

- ~~Firmware lists stop at 200 entries (and relationship lists, like an album's tracks, at 50)~~: State now loads a window of `LOOKUP_WINDOW` (64) names around the visible rows (`State::ensureWindow`, `MusicLookup` lookups take an offset), so lists have no length limit. Also fixed: the `getAll*` lookups never decremented `buffer_remaining` (overrun once names passed 8 KB), and a search hit in an album past its 50th track couldn't be played within the album.
- ~~I might want to make it a Rust app/CMD. Tauri? I would have it compiled for all platforms on github actions and copy it to the SD card to always have it. It would be nice to have a nice managment UI for adding/sharing/removing stuff to the lib. Let's see if we need a whole collection view(Itunes like) or just a nice folder selector + loading bar.~~
- ~~Could we have a emscripten version of the Rust backend and use it directly from the website? So you can add music just by visiting the website?~~
- ~~Add a `useFFMPEG` option, see if it makes the converting faster.~~
- ~~Check why my transfert is only 54Mb/s max, is it my card, usb-c adapter or software? (delays between each file etc)~~
- Make it not convert .m4a files: the vs1053 handles them well
- Add tests for all audio/video input files: wav, mp4, flac etc.
- During transfert the `on the card` number in the left panel does not update, it should
- Test the `Remove from the card` feature: does it remmember to ignore them from transfert next time? It should show that it is blaclisted in the `what s new` panel
- ~~Trying to remove things fom the Card while a sync is running breaks it.~~
- For podcasts: propose to auto remove the read ones.
- ~~The `read the card` feature is sloooow. Does it read one file at the time ? Does it read all the files ? It should read only the header or each, and multithread: fill up a buffer with all the metadata and have a pool of threads that read from it~~

## Podcasts and audiobooks

 I listen quite a lot of that we would need to:

- Have a script to normalize their ID3 tags ( just like I use Beet for the music ones), is there one for podcasts ?
- ~~We would need the indexer to crawl through them and index them~~
- ~~Would be nice to have subMenus for them. The difference with the music is that the tree might not have
  the same depth everywhere. ~~

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

- ~~We should save the listenned episodes and the current timestamp within unfinished episodes. ~~There should be some shortcut to mark an episode as read/unread from the player.
