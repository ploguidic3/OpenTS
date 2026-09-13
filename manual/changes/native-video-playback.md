---
title: Play a container movie in a VQA's place at the window's own size
category: feature
release: 0.2.0
targets:
- type: format
  id: video-files
  effect: added
- type: format
  id: vqa
  effect: changed
- type: key
  id: StretchMovies
  effect: changed
credit:
- Joe
---

A full screen movie is first looked for as `<name>.mp4` in the folders the game searches, and a file found there plays through Windows Media Foundation and the renderer at the window's drawable size rather than through the game's own 16-bit frame. A 4K encode of a briefing therefore shows at 4K on a 4K display. The picture follows the sound track's clock as a VQA's does; ESC, the skip vote in a network game, focus loss and the mission flow around the movie are unchanged.

Without such a file, or where the file will not open, the VQA plays exactly as before. The radar pane, the menu animations and the world map clips stay VQA. `StretchMovies=no` fits a container movie by a whole number instead of to the display's edge. [Video files](/formats/video-files/) covers naming, placement and what the decoder accepts.
