---
key: StretchMovies
summary: Scales a full screen movie to fit the display instead of playing it at its own size.
see_also: [ScreenWidth, ScreenHeight]
when_omitted:
  kind: value
  value: "no"
---

The flag reaches the full screen movie player alone, and only where that player's caller also asks for stretching. A movie played into a fixed rectangle — the sidebar's, for one — never consults the flag and keeps that rectangle either way.

A stretched movie keeps its shape. It grows by whichever of the two axes runs out first and sits centered in what is left over, so a 640 by 400 movie on a 1920 by 1080 display plays at 1728 by 1080 with a black band down each side. Where the two shapes match, as at 1280 by 800, the movie reaches every edge.

The screen is cleared ahead of any full screen movie that will not cover the display, stretched or not, whether or not its caller asked for a clear. The bands around such a movie are therefore black rather than whatever the display last held.

A [container movie](/formats/video-files/) is drawn into the window rather than the game's frame and consults the flag differently. `yes` grows it to the window's edge along whichever axis runs out first; `no` grows it by a whole number where the window is at least the movie's size, and fits it as `yes` would where the window is smaller. Either way it keeps its shape and sits centered.

The display options screen carries the same switch and stores it as the screen is accepted; leaving the options screen behind it writes the setting back to `sun.ini`.
