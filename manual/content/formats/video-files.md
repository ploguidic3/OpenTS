---
format_id: video-files
title: Video files
summary: Container movies that stand in for full screen VQAs and play at the window's own resolution.
kind: binary
extensions:
  - .mp4
role: video
source_files:
  - code/video/videomovie.cpp
  - code/intro.cpp
  - code/video/mfplayer.cpp
  - code/video/videosink.cpp
  - code/movie.cpp
  - code/video.cpp
related:
  - type: format
    id: vqa
  - type: format
    id: opents-ini
  - type: key
    id: StretchMovies
---

A container movie is a loose `.mp4` that plays in place of a full screen [VQA](/formats/vqa/). It is decoded by Windows Media Foundation and drawn by the renderer straight into the window, fitted to the drawable area, so it keeps its own resolution rather than being reduced to the game's frame and enlarged again.

## Naming and placement

The file is named for the movie it replaces, without the `.VQA` extension: `GDI_M02.mp4` stands in for `GDI_M02`, and `WWLOGO.mp4` for the `WWLOGO.VQA` the startup sequence names outright. Case does not matter. Whatever extension the movie name carries is dropped before `.mp4` is added.

Not every name is written into the code that asks for it. The cinema that opens a campaign's first mission is looked for as `INTR<n>.VQA`, where `<n>` is the campaign's CD number, and as `INTRO.VQA` only where no file of that name is present; the stock GDI campaign resolves to `INTR0`, so `INTR0.mp4` is what stands in for it and `INTRO.mp4` is never consulted. The [debug log](/using/debug-logging/) names every full screen movie the game asks for and the container file found for it, or records that none was found, so a movie that keeps playing as a VQA is a question the log answers.

The file is found the way any loose file is: the user directory, the current directory and then [the folders `SearchPaths` names](/formats/opents-ini/#the-order-files-are-searched-for-in), in that order. A deployment can keep its movies in a folder of their own by adding it to `SearchPaths`. A `.mp4` inside a MIX archive is never played.

## What plays this way

Every movie the [full screen player](/formats/vqa/) shows: the startup and title sequences, mission introductions and briefings, action, win, lose, post-score and map selection movies, the campaign's final movie and a movie a trigger or team mission names. The session test is the same, so a skirmish with the launch file's movies off shows neither kind.

Three uses keep to VQA whatever files are present: the movies that play inside the radar pane, the menu animations, and the world map clips between missions.

## What the decoder accepts

Media Foundation decodes H.264 video with AAC audio in an MP4 container on every edition of Windows 10 and later that carries the media components. HEVC and AV1 need the codec extensions from the Microsoft Store. The picture is taken at whatever size and frame rate the file declares; the sound track is taken at its own rate and mixed like any other sound, with any channels beyond the front pair dropped. Encode the sound at 44.1 or 48 kHz in stereo: an AAC track at 22050 Hz mono, the rate of the original VQA tracks, comes out of the Windows decoder wrong and plays as a stuttering picture with broken sound.

A file that does not open, or one that yields no picture, is passed over and the VQA of the same name plays instead, in the same request. A file that fails partway through ends where it fails and the VQA does not follow it. On an N edition or a server without the media feature no container movie plays and every VQA plays as before.

A movie narrower than 320 and shorter than 200 is passed over the way a VQA of that size is.

## How it plays

The picture follows the sound: the frame shown is the newest one due by what the mixer has heard of the track, less what the output device still holds, so the two stay in step through a stall on the game's own thread. A movie with no sound track is timed from the wall clock, and so is any movie until the first of its sound has been heard. When the movie ends its last frame is left in the game's own screen, reduced to fit, as a VQA's last frame is. Sound is decoded about half a second ahead of the picture; frames the clock has already passed are dropped rather than shown late.

The movie keeps its shape and sits centered in the window with black around it. `StretchMovies=yes` grows it to the window's edge along whichever axis runs out first; `StretchMovies=no` grows it by a whole number instead, so a 1920 by 1080 movie in a 3840 by 2160 window fills it either way while a 1280 by 720 one shows at twice its size with a border. The window's size is what counts, not the resolution the game draws at, so a full screen window on a 4K display shows a 4K movie at 4K.

ESC ends the movie, and in a game against other machines it casts the same vote a VQA takes; [multiplayer movies](/systems/multiplayer-movies/) covers the vote. The vote's status text is drawn over the movie as it is over a VQA, grown with the picture. While the window is out of focus the movie and its sound wait, unless the session is a network game, where the other machines are waiting on this one.
