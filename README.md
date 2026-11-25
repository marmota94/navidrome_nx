# Navidrome NX

A Nintendo Switch homebrew music client for Navidrome, built with **Aurore + Cadran**
and the Subsonic API. Includes a separate **Ultrahand mini-player** backed by
sys-tune for background playback.

Independent third-party software, not affiliated with or endorsed by Navidrome.

## Features

- **Home:** horizontal artwork shelves for Most played, Favorites, Play again,
  Discover, and Random songs. Each shelf contains up to 12 items.
- **Library:** Artists, Albums, Songs, Playlists, and Favorites; artwork, search,
  ordering, List/Grid/Cover Flow views, and bottom pagination. Album lists open at the first
  song; returning preserves the parent page and selection.
- **Playback:** previous/next, play/pause, stop, automatic queue advancement,
  elapsed/total time, album art, and a waveform driven by the mixed audio.
- **Visualizers:** Waveform, Bars, and stereo VU meter, selected in Settings.
- **Lyrics player:** centered lyrics, timed highlighting with a small enlargement
  and bold transition, automatic follow, manual scrolling, and three text sizes.
- **Touchscreen:** tap controls and music cards; swipe lists and lyrics vertically
  and Home shelves horizontally. Controller navigation remains available.
- **Song cache:** optional SD cache of up to 15 recently used songs.
- **Android remote:** browser controls on the same Wi-Fi, paired using a QR code.
- **Appearance:** animated Bubbles or XMB waves, background opacity, a subtle
  three-minute Rainbow cycle, AMOLED black with optional Warp, and a collapsible sidebar.
- **Localization:** English and Spanish selected from the Switch system language.

Artists, Albums, and Songs have separate search/order/view choices while the app
is open. Collections use pages of 50. Artist/song sorting covers the complete
matching result, up to 50,000 entries; initial loading can take longer on large
libraries. Playlists queue their complete server-ordered track list. Favorites
includes **Shuffle all** across all favorite-song pages. Playlist editing and
changing stars remain in Navidrome. Favorite-song rows include album-art
thumbnails next to their track numbers.

Home's Most played and Play again shelves use the server's frequent/recent album
history; Discover uses random albums. Empty or unavailable sections show a
message. Refresh updates the current page or Home recommendations.

## Build the app

Docker must be installed and running; no host Switch SDK is required. The build
uses `devkitpro/devkita64:20260219`, libnx, SDL2_mixer, curl, Jansson, and mbedTLS.
Vendored dependencies are included in a normal clone; no submodules are needed.

```sh
bash scripts/build.sh
```

Output: `navidrome_nx.nro` in the repository root.

For a self-contained Docker artifact export:

```sh
docker build --target artifact --output type=local,dest=dist .
```

Output: `dist/navidrome_nx.nro`. This export does not update the root NRO; copy it
before using the nxlink script:

```sh
cp dist/navidrome_nx.nro navidrome_nx.nro
```

For native Aurore screen and interaction checks (no Switch required):

```sh
docker build -t navidrome-aurore-ui-test -f tests/ui.Dockerfile .
docker run --rm -v "$PWD:/app" navidrome-aurore-ui-test
```

Rendered screens are saved under `build.ui/screenshots/`.

## Equalizer

Open **Settings → Audio** to adjust bass, mids, and treble from −6 to +6 dB.
Changes apply during playback and are saved automatically. **Reset equalizer**
returns all bands to 0 dB.

## Download management

Open **Settings → Downloads**, select one or more cached songs, and choose
**Delete selected**. Older cache entries display their file name until they
are played again. The Ultrahand player has its own cache.

## App updates

Navidrome NX checks recent GitHub releases at startup and offers the newest
stable release that includes the app.
Open **Settings → Account** to check again or install an available update.
When the download is ready, quit Navidrome NX with **+** to apply the update,
then reopen it. The previous executable is
kept as `navidrome_nx.nro.bak` in the app directory.

The updater installs the NRO at `/switch/navidrome_nx/navidrome_nx.nro`.
Install the first updater-enabled build there manually. Install Ultrahand
and sys-tune packages separately.

For a future app release, build with the matching version and attach
`dist/navidrome_nx.nro` to the corresponding stable tag (for example, `v1.1.1`).
The updater verifies the asset size and the SHA-256 digest from GitHub.

```sh
docker build --build-arg NAVIDROME_VERSION=1.1.1 --target artifact --output type=local,dest=dist .
```

## Install and sign in

1. Copy the NRO to `/switch/navidrome_nx/navidrome_nx.nro` on the SD card.
2. Launch from Homebrew Menu in full-memory mode (title takeover), rather than
   Album applet mode.
3. If `/switch/navidrome_nx/config.json` is missing or invalid, the login screen
   asks for **Server URL**, **Username**, and **Password** using the Switch keyboard.
4. Select **Test connection**, then **Save and connect** after the test succeeds.
   Editing a field disables Save until another successful test.

An existing valid config opens Home directly. A missing main config opens login
even if an old backup exists. Manual setup using `config.example.json` is also
supported; restart after manually changing credentials.

Use the final server URL, including any prefix such as
`https://music.example.com/navidrome`, without appending `/rest`.
TLS verification remains enabled and redirects are rejected. Requests use salted
Subsonic token authentication; HTTPS protects tokens and music in transit.
Passwords are masked in the UI but stored as plain text in the SD-card config.
Real config and backup files are excluded from Git and the Docker build context.

**Settings → Account** shows the connected server URL and username, plus Log out.
Logout stops app playback, disconnects the web remote, clears the app session,
removes saved login files, and returns to login. Appearance preferences and
previously downloaded cache files remain on SD.

## Controls

| Input | Action |
| --- | --- |
| A | Select the focused item |
| B | Back / return to the parent page |
| X | Play / pause |
| L / R | Previous / next song; previous / next tab in Settings |
| Y | Refresh the library or Home; toggle follow on the lyrics screen |
| ZR | Open the lyrics player |
| − | Collapse / expand the sidebar |
| Left / Right | Browse a Home shelf or adjust a focused setting |
| Up / Down | Move between items; browse lyrics manually |
| Tap | Activate a visible button, navigation item, or music card |
| Swipe | Scroll lists/lyrics vertically or Home shelves horizontally |

Unavailable player/page controls are disabled. Swiping and multitouch do not
activate song rows. Lyrics ignore swipes and directional scrolling by default. Enable their
**Manual scrolling** button to browse using swipes or Up/Down; B leaves manual
mode, and pressing B again returns to the library. Turning manual mode off
resumes follow for timed lyrics. Text-size changes use the dedicated button. Text-size and playback controls use icons with translated
controller hints. Icons in the mini-player and lyrics buttons use visible glyph
bounds for centering rather than the surrounding text font's line metrics.

## Cover Flow

The view button cycles **List → Grid → Cover Flow** on Artists, Albums, Songs,
Favorites (songs and albums), Playlists, artist albums, album tracks, and playlist
tracks. Each screen keeps its view choice while the app is open; Home retains its
horizontal shelves.

Cover Flow displays a large centered cover, angled neighbors, short mirrored
reflections, and the selected title/artist. Left/Right browses; A opens the centered
artist/album/playlist or plays the selected song using the current page/track queue.
Swipe horizontally to browse. Tap a neighboring cover to center it, then tap the
center cover to open/play. Up returns to the view button; Down reaches the page
controls or mini-player. Existing paging, search, ordering, and Favorites Shuffle
all remain available where applicable.

Only the seven neighboring cover slots are loaded, with shared images when
tracks have the same artwork. Missing art uses a placeholder. Returning from a
detail page restores the chosen cover; opening a new album starts at its first
song. The Cover Flow rendering, touch hit areas, controller focus, and performance
still require Switch verification.

## Settings

| Tab | Options |
| --- | --- |
| Appearance | Background color, opacity, Bubbles/XMB, bubble count, speed |
| Layout | Clock, 4–6 items per grid row, sidebar collapse, song caching, visualizer |
| Remote | Enable the web remote, address, pairing code, QR |
| Account | Connected URL and username, Log out |

Settings persist in `/switch/navidrome_nx/settings.json`. AMOLED uses pure black
without effects. **AMOLED Warp** adds a sparse starfield on black, with stars
and short trails moving outward from the center. Its Animation speed setting
controls motion; Off leaves a static starfield. Bubbles and XMB are hidden in
this mode. Rainbow cycles smoothly through muted colors over 180 seconds.
Colored backgrounds use a broad, slowly drifting radial glow, with subtle
dithering to soften visible gradient bands. The original XMB wave ribbons are
retained; animation speed controls their motion (Off pauses it). Settings tabs
wrap with L/R, and the upper-right Back control uses a centered arrow icon.
Opacity adjusts color intensity over black (0–100%, default 80%). Bubbles bounce
at screen edges and become smaller as their count increases (0–10). The clock
uses Switch local time. Irrelevant appearance controls hide automatically.

### Mini-player visualizer

Choose **Settings → Layout → Mini-player visualizer**: Waveform (default), Bars,
or VU meter. The choice is saved and applies immediately. Waveform and bars use
short-window audio peaks; bars represent amplitude, not a frequency spectrum.
The VU style shows independent left/right RMS levels on a −48 to 0 dBFS scale,
with green/yellow/red segments and fast attack / slower release. Mono output is
mirrored to both rows. All styles react to actual mixed audio, freeze on pause,
and clear on stop. Capture does not allocate, lock, or modify the audio stream.
The new styles require Switch visual/performance verification.

### Song cache

Enable **Settings → Layout → Cache songs**; it is off by default. Played songs
are saved under `/switch/navidrome_nx/song-cache` and reused before downloading
again. Older entries are evicted to keep a maximum of **15 songs total**.
Server/account-specific hashed filenames keep accounts separate. Files contain
MP3 audio, not credentials. Storage failures show a warning and leave playback
working when the song can still be fetched.

Disabling caching stops cache reads/writes without deleting existing files.
Stop the app and delete `song-cache` to erase downloaded music, including after
logout. The existing 64 MiB per-track limit allows up to 960 MiB for 15 tracks.
This speeds repeated playback; it does not add an offline library browser or
control the separate Ultrahand cache.

## Artwork and lyrics

Visible artwork loads outside the UI thread with a 64-image RAM cache. The
player requests 768px artwork; grid cards request 256px. Quality depends on the
source image. Responses are capped at 512 KiB and decoded dimensions at 1024×1024.
Missing/invalid art uses a placeholder without stopping audio.

Both players show source format, bitrate, bit depth, sample rate, and channels
when supplied by Navidrome. These describe the source; playback requests MP3
at up to 192 kbps. Unknown metadata is omitted.

Lyrics use `getLyricsBySongId`, with legacy artist/title lookup as a fallback.
The first available version is displayed. Timed lines follow the estimated
playback clock and server offset, enlarge by 8%, and fade into bold over 250 ms.
Plain lyrics remain manually scrollable. Missing/invalid timestamps disable
follow. Responses are limited to 256 KiB and 2,000 lines. Music continues when
entering or leaving the lyrics player.

## Android web remote

1. Connect the phone and Switch to the same Wi-Fi.
2. Enable **Settings → Remote → Web remote**.
3. Scan the displayed QR code to open the browser remote and pair automatically,
   or open its URL and enter the displayed code manually.

Controls include previous/next, play/pause, stop, song search, favorites, and
Shuffle all. Audio plays on the Switch and the app must remain open. Navidrome
credentials are never sent to the browser. Pairing changes when the app restarts
or the remote is re-enabled. Disconnect clears the browser's saved pairing.
The remote uses local HTTP on port 8080; use trusted Wi-Fi and do not forward it
to the internet. Guest-network isolation can prevent access.

## Ultrahand mini-player

```sh
./scripts/build-overlay.sh
```

Outputs `dist/NavidromeNX-ultrahand.zip` and the extracted SD layout in
`dist/ultrahand/`. The package includes `NavidromeNX.ovl` and a pinned sys-tune
2.0.0 / IPC API 4 audio module. It does not include Ultrahand or Atmosphère.

The overlay provides artwork, actual playback progress, transport controls, and
Favorites → songs, Albums → songs, and Artists → albums → songs. It reads the
login saved by the app. Selecting a song starts playback when that song is
cached. The next two songs are cached and queued in the background. sys-tune
can continue playing queued songs with the overlay closed.

This is a **separate queue** from the Aurore app and Android remote. It does
not provide unlimited Navidrome streaming in the background. See
[installation, controls, cache limits, licensing, and hardware-test status](docs/ultrahand.md).
The [original feasibility assessment](docs/ultrahand-feasibility.md) records a
proposed future unified service architecture, not the current implementation.

## Test on Switch with nxlink

Start Homebrew Menu's network loader with Y, then run:

```sh
bash scripts/nxlink.sh 192.168.1.91
```

Replace the address with your Switch's IP. The script uses the root NRO, uploads
it to `/switch/navidrome_nx/navidrome_nx.nro` alongside the config, and listens
for console output. Docker host networking needs access to the Switch's LAN.
nxlink transfers the executable, not the config. Ctrl+C ends the host listener.
The overlay/sysmodule package is installed separately as described above.

## Automated checks and validation

```sh
docker build -t navidrome-nx-test -f tests/Dockerfile .
docker run --rm -v "$PWD:/app:ro" navidrome-nx-test
```

Tests use mock servers and temporary files; no real credentials are needed.
They cover authentication, escaping, API errors/cancellation/byte limits, library
paging and sorting, artwork, lyrics, clock timing, settings/login persistence,
translations, remote authorization and QR decoding, audio waveform capture,
15-song cache eviction/account separation/storage failures. The Aurore harness
checks native touch/controller interactions, login, library views, lyrics, and Settings
in English and Spanish. Overlay tests cover browsing and cache
protection/cancellation; its build script checks the exported NRO/ExeFS headers,
sysmodule program ID, and boot flag.

**Verified locally:** app and overlay Docker builds, automated tests, and package
checks. **Previously confirmed on Switch:** library/playback, Home shelves,
sidebar/icons, lyrics navigation, web remote QR pairing, Settings tabs,
search/order/list-grid controls, pagination, playlists, and waveform behavior.

The app now runs on Aurore. See [migration notes](docs/aurore-migration.md)
for build details and the hardware verification status.

**Ultrahand hardware validation is pending:** in-game audio mixing, overlay
memory use, suspend/resume, and firmware / Atmosphère / Ultrahand compatibility
have not been established by local tests or the app's Switch confirmation.

## Limitations

- One complete MP3 is downloaded or read from cache into RAM before playback,
  at most 64 MiB. No progressive streaming, seeking, scrobbling, or offline
  library browser is implemented.
- Navidrome must support MP3 transcoding for non-MP3 sources.
- Lyrics synchronization uses an estimated playback clock, without word-level
  karaoke or audio-device latency adjustment.
- Aurore uses its null UI audio driver on Switch so SDL owns the audio renderer.
- The app's playback ends when the app exits; background playback belongs to
  the separate sys-tune overlay queue.

## Languages and source layout

English and Spanish follow the console language at startup; `es-419` uses Spanish
and other languages fall back to English. Restart after changing system language.
Catalogs are in `resources/i18n/<locale>/app.json`; retain the keys and format
placeholders when translating. Music metadata and lyrics remain server-provided.

| Path | Contents |
| --- | --- |
| `src/` | Aurore app, API, playback, settings, remote, cache |
| `overlay/` | Native Ultrahand player and browsing/cache code |
| `resources/` | Fonts, artwork, translations, browser remote |
| `vendor/` | Included dependencies and license notices |
| `scripts/` | Docker builds, package checks, nxlink, font generation |
| `tests/` | Native tests, mock server, test Docker image |
| `docs/` | Overlay installation and architecture notes |
| `config.example.json` | Placeholder SD-card configuration |

Build artifacts and real credentials are ignored by Git. Normal builds use the
included fonts; regeneration scripts require Python fonttools.

## Upstream and notices

- [Aurore](https://github.com/JigSawFr/Aurore), v1.1.0:
  `e8ea3dcfc194c9b0271f064ba67cfe462c9baad1`. Core Apache-2.0; Cadran
  GPL-3.0-or-later. The full-screen app is GPL-3.0-or-later; see [LICENSE](LICENSE)
  and [NOTICE](NOTICE). Vendored components retain their own licenses.
- [libultrahand](https://github.com/ppkantorski/libultrahand):
  `548e491e19d791120ad3de05b4a7efea4ca00e34`, included GPL-2.0 / sub-license notices.
- [sys-tune](https://github.com/HookedBehemoth/sys-tune):
  `24132cfa26e569011a2028790575cd474f32cf95`, GPL-2.0. The module uses its original
  service/title ID; do not install a second copy beside an existing sys-tune.
- [Nayuki QR encoder](https://www.nayuki.io/page/qr-code-generator-library):
  v1.8.0, MIT, with revision and license in `vendor/qrcodegen`.
- Inter-derived fonts retain their SIL notices. The lyrics bold font uses Inter
  v4.1, `e3a3d4c57d5ecc01453a575621882a384c1995a3`. Material Icons retains its
  included license. New overlay sources are GPL-2.0-or-later.

The header and launcher currently use the
[official Navidrome logo](https://www.navidrome.org/brand/navidrome-logo.png).
Navidrome's [brand guidelines](https://www.navidrome.org/brand/) require written
permission for third-party app-icon distribution; this repository does not grant
that permission. Upstream license notices remain with the included sources/assets.

API references: [Navidrome Subsonic API](https://www.navidrome.org/docs/developers/subsonic-api/),
[OpenSubsonic endpoints](https://opensubsonic.github.io/docs/).
