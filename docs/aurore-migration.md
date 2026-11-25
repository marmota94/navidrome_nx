# Aurore UI migration

Branch: `feat/aurore-ui`. The complete Borealis baseline, including pending
visualizers, AMOLED Warp, and Cover Flow, is preserved at commit `1e1e3a9`.
Check out that commit in a separate worktree to build the previous UI. Existing
SD configuration, settings, cache, API contracts, and remote pairing are unchanged.
Do not run two app versions against the same SD directory at the same time.

## Framework and design

The full-screen application uses Aurore v1.1.0 at
`e8ea3dcfc194c9b0271f064ba67cfe462c9baad1`, vendored without optional platform
submodules. CMake builds GNU C++23 and the Switch deko3d backend. No Borealis
headers or implementation remain in the active build.

All screens use Aurore activities, layout, labels, images, focus/actions, IME,
scrolling, and touch recognizers. Library cards and screen panels use Cadran's
card kit; MusicButton extends native IconButton to switch between existing
localized text and Material glyphs. The standalone Cadran screen hook preserves
the existing sidebar, backgrounds, mini-player, and custom lyrics/Cover Flow
renderers rather than adding a second navigation frame. Text size stays normal
in both docked and handheld layouts.

Login still tests credentials before enabling Save. Input uses Aurore's platform
IME. Navigation uses native directional actions; native pan/tap recognizers handle
Home shelves, cards, Cover Flow, and optional manual lyrics scrolling. Album art
keeps its existing input-size/dimension limits before native texture upload.
Lyrics artwork flexes to leave room for metadata and controls.

Resources combine Aurore's fonts and translations with the application's assets;
Spanish app strings are also copied to the `es-419` resource directory. The
separate Ultrahand player remains on libultrahand/sys-tune. It only shares the
vendored stb image header with the app, not Aurore's runtime.

## Local upstream changes

The two deko3d fill fragment shaders add stable half-step dithering only to
opaque gradients with unequal colors. This retains the previous application's
smooth gradients while leaving plain AMOLED black and translucent waves alone.
Aurore recompiles these shaders with the SDK's uam compiler. The native label layout refreshes text metrics even when Yoga skips measurement
for fixed width/height fields; this prevents metadata overflow and stale ellipses.
Other framework sources retain the pinned release implementation. See `vendor/aurore/UPSTREAM.md`
for the test harness PNG writer's source and license.

The application uses Cadran and is GPL-3.0-or-later. Aurore core and all other
included components retain their licenses. The separate overlay/sysmodule retain
their GPL-2.0 licensing; they do not link Cadran.

## Validation

Use the README's Docker commands for the NRO, pure API/model tests, and native
Aurore headless UI tests. The headless test uses a local fake Navidrome server,
keyboard responses, touch/controller input, software rendering, and screenshots
in English and Spanish. It exercises production screens and playback with a
synthetic audio fixture; it does not contact a user's server.

A new Switch run is still required to verify deko3d rendering, touchscreen,
SDK keyboard, audio ownership, performance, sleep/resume, and actual screen
legibility. Desktop headless results do not establish those hardware behaviors.
The v1.0.0 release remains available as the Borealis baseline.

Validated locally on 2026-11-16: Switch Docker NRO build and embedded metadata;
API/model/authentication/cache/remote/QR/lyrics/background/Cover Flow tests;
production-screen Aurore headless walkthrough in `en-US` and `es-419`, including
native keyboard, touch, controller navigation, search/order/pagination, sidebar,
playback, manual lyrics gating, all Settings tabs, background choices, and remote
pairing display; Ultrahand Docker export and package identity/boot checks.
Screenshots are generated under `build.ui/screenshots/` and are not committed.
