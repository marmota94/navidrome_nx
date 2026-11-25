# Navidrome NX for Ultrahand

Native overlay with album artwork, real playback progress, previous/next,
play/pause, Favorites → songs, Albums → songs, and Artists → albums → songs.
English and Spanish follow the console language. Lists are paged and retain the
selected row when returning from an album or artist.

The overlay uses **sys-tune 2.0.0 / IPC API 4** for background audio. The included
sys-tune module is built from pinned upstream source; it plays local files.
Navidrome tracks are downloaded to SD as MP3 up to 192 kbps before queueing.
Closing the overlay leaves sys-tune playing. Network access and downloads run
on a worker, not in the overlay drawing/input loop.

## Build

Docker is the only host build dependency:

```sh
./scripts/build-overlay.sh
```

Output: `dist/NavidromeNX-ultrahand.zip` and the extracted SD layout in
`dist/ultrahand/`. This does not change the regular NRO build.

Equivalent artifact-only build:

```sh
docker build --target overlay-artifact --output type=local,dest=dist/ultrahand .
```

## Install

1. Install a working Atmosphère setup and Ultrahand overlay loader first.
2. Sign in with the normal Navidrome NX app. The overlay reads the existing
   `/switch/navidrome_nx/config.json`; it does not have a separate login.
3. Stop playback and exit the normal app before using this player.
4. Copy `switch/` and `atmosphere/` from the built package to the SD root.
   The overlay goes to `/switch/.overlays/NavidromeNX.ovl` and the audio module
   to `/atmosphere/contents/4200000000000000/`. If sys-tune is already installed,
   back up that folder first: this is the same upstream service and title ID,
   not a second module to install beside it.
5. Reboot the console to start sys-tune, then open Navidrome NX in Ultrahand.

If the module prevents booting, remove its `boot2.flag` from that contents
folder on the SD card to disable automatic startup.

## Controls

- A: select a row; the transport row toggles play/pause.
- L / R: previous / next track from any page.
- X: play/pause from any page.
- B: return to the previous list; from Home, close/return to Ultrahand.
- Stop and clear queue: release the current queue, including its cache protection.

Select a song to download it and start playback as soon as it is ready. The
overlay then caches and queues up to two following songs, in order, even when
they appear on the next page. Keep the overlay open until caching finishes. Once
queued, those songs can play with the overlay closed or Wi-Fi disconnected.
Select another song to start a new set. A failed selected download keeps the
previous queue. If an upcoming download fails, already queued songs can still
play. Selecting another list or song takes priority over background caching.

## Limits and storage

- This is a separate player from the Aurore app and Android web remote; those
  continue to use the app's own queue. They do not control sys-tune.
- Cache: `/switch/navidrome_nx/overlay-cache`, at most 256 MiB of MP3 files,
  maximum 24 MiB per track. Files are written directly to SD using bounded curl
  buffers. No whole song is held in overlay RAM.
- Cache filenames are hashes of the account/server and song ID. Metadata sidecars
  contain titles and cover IDs, never passwords or authenticated URLs.
- Old cache entries are evicted before downloads; current/queued tracks are
  protected. If the protected queue fills the budget, use Stop and clear queue
  before downloading another song. Interrupted partial downloads are cleaned up
  at the next cache reservation.
- Cache persists across reboot/logout. To erase saved music, stop sys-tune and
  delete `overlay-cache` on the SD. Signing out in the app prevents new overlay
  requests; it does not stop an already cached sys-tune queue.
- Artwork: bounded 128 KiB download, at most 256×256 source pixels, rendered at
  128×128. Missing or invalid artwork uses a placeholder and does not interrupt
  music. Artwork requires network access when the current track changes.
- Favorites responses are downloaded to a temporary SD file (up to 8 MiB) and
  parsed one song at a time, retaining only the displayed ten-song page. The
  temporary file is removed on success or failure. Other library responses are
  capped at 512 KiB; extremely large artist indices can exceed that limit.

## Validation status

Docker builds the `.ovl` and `exefs.nsp`. Native tests cover authenticated
browsing, pagination, account-separated filenames, streamed writes and byte
limits, cache eviction/protected tracks, metadata, and cancellation cleanup.
The package check verifies the NRO magic, ExeFS contents, sysmodule program ID,
and boot flag.

On November 18, 2026, the user confirmed that Favorites opens successfully on
Switch after installing the response-memory fix via FTP. This confirms the
reported Favorites failure is resolved on that console. In-game audio mixing,
broader firmware / Atmosphère / Ultrahand compatibility, suspend/resume,
repeated open/close, artwork, and controller navigation still need dedicated
hardware validation.

## Upstream and licensing

- [libultrahand](https://github.com/ppkantorski/libultrahand), pinned at
  `548e491e19d791120ad3de05b4a7efea4ca00e34` (GPL-2.0 / upstream sub-license).
- [sys-tune](https://github.com/HookedBehemoth/sys-tune), pinned at
  `24132cfa26e569011a2028790575cd474f32cf95` (GPL-2.0).

Source and license notices are in `vendor/libultrahand/` and `vendor/sys-tune/`.
The new overlay sources are GPL-2.0-or-later. The sys-tune NPDM configuration
uses modern key names without changing its title ID or service protocol.
