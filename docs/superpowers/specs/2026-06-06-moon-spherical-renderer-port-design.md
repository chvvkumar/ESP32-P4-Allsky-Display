# Spherical Moon Renderer Port (NINA to Allsky)

Date: 2026-06-06
Status: Approved (design), pending implementation plan

## Goal

Replace the Allsky display's current flat moon renderer with the spherical,
texture-mapped renderer developed in the ESP32-P4-NINA-Display project. Bring
over the full astronomical calculation set (libration, sub-solar point, axis
orientation), the tgx-based 3D sphere renderer, the equirectangular surface
texture, interactive drag-to-rotate, and the expanded configuration set.

A specific product requirement: when the user scales the moon down using the
existing per-source scale control in the web UI, the starfield and/or glow
background fills the area between the moon disk and the edge of the screen.

## Source and target

Source (reference implementation):
- `C:\Users\Kumar\git\ESP32-P4-NINA-Display\main\moon_ephemeris.c/.h`
- `C:\Users\Kumar\git\ESP32-P4-NINA-Display\main\moon_sphere.cpp/.h`
- `C:\Users\Kumar\git\ESP32-P4-NINA-Display\main\moon_interaction.c/.h`
- `C:\Users\Kumar\git\ESP32-P4-NINA-Display\main\moon_equirect.jpg` (2048x1024 grayscale)

Target:
- `c:\Users\Kumar\git\ESP32-P4-Allsky-Display\` (flat Arduino sketch)

Environment differences that drive the design:
- NINA is an ESP-IDF project. It embeds assets with CMake `EMBED_FILES` and
  decodes with `stb_image`. Allsky is a flat Arduino sketch compiled with
  arduino-cli. Asset embedding here uses a C byte-array header, and JPEG decode
  uses the project's existing `JPEGDEC` path.
- NINA renders the moon on a dedicated touch page. Allsky shows the moon as one
  source inside a timed image slideshow.

## Confirmed device resources (live, 2026-06-06)

Measured from the running device at `http://192.168.1.123:8080/api/info` and
`/api/health`. The device is the 800x800 (4.0 inch) panel, display_type 1.

- PSRAM total: 33,554,432 bytes (32 MB).
- PSRAM free while running: ~11.9 MB (64.5 percent in use).
- Flash: 32 MB total, app partition free ~10 MB, current sketch ~1.96 MB.
- Internal heap free: ~331 KB of 551 KB.
- Chip: ESP32-P4, 2 cores at 360 MHz.

Conclusion: the 32 MB PSRAM removes the earlier budget risk. The full
2048x1024 RGB565 texture (4 MB resident) plus the existing `fullImageBuffer`
(4 MB) and `scaledBuffer` (~5 MB) fit with margin. No texture downscale or
buffer reuse fallback is required.

## Architecture

Five units, each with a single responsibility.

### 1. moon_ephemeris.c/.h (upgrade in place)

Replace Allsky's simplified ephemeris with NINA's extended version. The
`moon_state_t` struct gains:
- `lib_lon`, `lib_lat`: optical libration in longitude and latitude (radians).
- `sun_lon`, `sun_lat`: sub-solar selenographic point (radians).
- `roll`, `axis_P`: disc roll for the renderer and position angle of the lunar
  axis (radians).

The public `moon_compute(time_t utc, double lat, double lon, moon_state_t*)`
signature is unchanged, so callers continue to work. The algorithm follows
Meeus, chapters 48 and 53 (phase, illumination, bright-limb position angle,
libration, parallactic angle). Pure math, no display dependencies.

### 2. tgx/ (vendored library)

The tgx 3D template library (https://github.com/vindar/tgx) copied into the
sketch folder as `tgx/`. arduino-cli resolves it locally, matching how the
project already vendors its own headers. tgx is header-only, so no separate
compilation unit is added. A minimal compile check is performed early to
confirm it builds for the esp32p4 FQBN.

### 3. moon_equirect_data.h (asset)

The 2048x1024 grayscale equirectangular JPEG converted to a
`const uint8_t MOON_EQUIRECT_JPG[]` byte array (~500 KB in flash). Decoded once
at startup by `JPEGDEC` into a PSRAM RGB565 buffer (2048x1024x2 = 4 MB). The
decoded buffer is held for the application lifetime.

Texture orientation matches NINA: top row is lunar north (+90 latitude),
column center is the prime meridian (sub-Earth longitude 0). Runtime flips
`moon_flip_u` and `moon_flip_v` mirror columns or rows.

### 4. moon_sphere.cpp/.h (renderer, ported)

Ported from NINA's `moon_sphere.cpp`. Responsibilities:
- Texture init, RGB565 conversion, and U/V mirroring.
- Sphere tessellation and orthographic projection via tgx.
- Model matrix composing libration, base orientation, and parallactic roll.
- Sub-solar-point Lambert lighting (warm tint, low ambient for a dark
  terminator). Light modes: true-phase and explore (headlight).
- Background fill: black, deterministic starfield, warm glow halo, or both.

Adapted for Allsky so the disk is sized within a full-screen frame. See the
scale and background-fill section below.

Render entry points:
- A resting render at native display resolution (no user rotation).
- An interactive render at reduced resolution with yaw/pitch, for drag.
- An into-buffer variant that takes caller-provided persistent color and Z
  buffers to avoid per-frame allocation during drag.

### 5. moon_interaction.c/.h (drag state, ported)

Ported from NINA. A state machine that converts touch motion into eased
yaw/pitch, with snap-back or free-spin-then-settle on release. Driven by
Allsky's GT911 touch instead of NINA's page input.

### Removed

`moon_render.cpp` and `moon_texture_data.h` (the old 256x256 LA8 flat renderer)
are deleted once `moon_sphere` is in place.

## Display resolution handling (both panels)

The firmware supports two square DSI panels, selected at runtime by the Display
Hardware setting on the settings page:

- `SCREEN_4_INCH_CONFIG`: 800x800 (`displays_config.cpp`).
- `SCREEN_3_4_INCH_CONFIG`: 720x720 (`displays_config.cpp`).

`display_manager.cpp` chooses the active config from
`configStorage.getDisplayType()` during `begin()`, then exposes the resolved
panel size through `getWidth()` and `getHeight()`, which return
`gfx->width()` and `gfx->height()` after panel init. These are authoritative for
whichever panel is active.

Rules for the moon renderer so both resolutions work:

- The resting render reads `displayManager.getWidth()` and
  `displayManager.getHeight()` at render time and allocates its color and Z
  buffers at that size. No fixed 600, 720, or 800 constant is baked into the
  moon path.
- Disk-fraction sizing uses `min(width, height)` as the reference dimension.
  Both panels are square, so `width == height`, but using `min` keeps the math
  correct if a non-square panel is ever added.
- The interactive drag render stays at a fixed small size (240x240) and
  PPA-upscales to the active full-screen size. The upscale factor differs per
  panel (240 to 800 is 3.33x, 240 to 720 is 3.0x); PPA handles arbitrary scale,
  so the code passes the active dimensions rather than a fixed factor.
- The equirectangular texture is resolution-independent and shared by both
  panels. It is decoded once regardless of the selected display.
- Changing the Display Hardware setting requires a restart (existing behavior).
  Because the renderer reads the dimensions at render time, it picks up the new
  panel size on the next boot with no moon-specific change needed.
- `handleAddPreset` fit defaulting and the scale migration both read the active
  dimensions from `displayManager`, so they produce correct values on either
  panel.

## Scale and background fill

This is the core product requirement.

Current behavior: `renderMoonToPendingBuffer()` renders the moon at 600x600,
then the slideshow pipeline PPA-scales that 600x600 image (including its
background) to fit the screen. Scaling down leaves uncovered screen area, so the
background does not reach the edge.

New behavior:
- `renderMoonToPendingBuffer()` renders at native display resolution (800x800 on
  this unit, 720x720 on the 3.4 inch unit) directly.
- The per-source `scaleX` value controls the moon disk diameter as a fraction of
  the screen, implemented by widening tgx's orthographic half-extent:
  `ORTHO_R = 1.08f / scaleX`. So `scaleX = 1.0` makes the disk fill the screen
  edge to edge, and `scaleX = 0.5` makes the disk half the screen.
- The starfield and glow background are painted across the entire output buffer
  before the sphere is drawn, so they always fill the frame regardless of disk
  size.
- The moon source's pipeline transform is forced to identity scale so the PPA
  does not re-scale and double-apply. Per-source `offsetX`/`offsetY` still pan
  the moon by shifting the draw position.
- `handleAddPreset` for `__moon__` sets the default `scale` to 1.0 (disk fills
  screen) instead of `displaySize / 600`.

### Config migration

Because scale semantics change, a one-time migration resets existing `moon://`
sources to `scaleX = scaleY = 1.0` on first boot of the new firmware. The
migration is guarded by a config version bump so it runs once. Non-moon sources
are untouched.

## Interactive drag-to-rotate

Allsky's touch currently maps single-tap to next-image and double-tap to
mode-toggle. Drag must coexist.

- While the moon source is displayed, `updateTouchState()` measures finger
  travel from the press point. Travel past a threshold (about 12 px) classifies
  the gesture as a drag rather than a tap. A tap with travel under the threshold
  keeps the existing next-image behavior.
- On drag start, set `interactiveMoonMode = true` and pause cycling using the
  existing pause-flag pattern. Drag deltas feed the `moon_interaction` easing
  model as yaw and pitch.
- Each interactive frame renders small (240x240) with the disk sized to the
  current scale, then PPA-upscales to full screen for roughly 15 to 20 fps, the
  same approach NINA uses.
- On release, snap-back or free-spin-then-settle per `moon_spin_mode` and
  `moon_spin_return_s`. After the moon settles and a short idle timeout elapses,
  `interactiveMoonMode` clears and the slideshow resumes.
- `moon_drag_light_mode` selects true-phase lighting (follows the sub-solar
  point) or explore lighting (headlight for inspecting the far side during
  drag).

## Web UI and configuration

Extend `config_storage` and the Moon settings panel with the full NINA option
set. New persisted fields, with NINA default values:

- `moon_bg_style` (0 black, 1 stars, 2 glow, 3 both)
- `moon_lat`, `moon_lon` (degrees, 0 means unset, north-up fallback)
- `moon_flip_u`, `moon_flip_v` (0 or 1, default 0)
- `moon_roll_offset` (degrees, clamp [-180, 180], default 0)
- `moon_yaw_offset` (degrees, clamp [-180, 180], default 0)
- `moon_pitch_offset` (degrees, clamp [-90, 90], default 0)
- `moon_north_up` (0 true sky tilt, 1 always upright, default 1)
- `moon_drag_light_mode` (0 true phase, 1 explore, default 0)
- `moon_spin_mode` (0 snap-back, 1 free spin, default 0)
- `moon_spin_return_s` (free-spin hold seconds, [3, 60], default 3)

`lat`, `lon`, and `bg` already exist and keep their current keys. The
`/api/getMoon` and `/api/setMoon` handlers gain the new fields. NVS persistence
adds the new keys with migration defaults.

## Data flow

Resting render (once per slideshow refresh of the moon source):

1. Main loop reaches the moon source. URL starts with `moon://`.
2. `renderMoonToPendingBuffer()` calls `moon_compute(now, lat, lon, &st)`.
3. `moon_sphere_render(displayW, displayH, &st, ..., bg_style)` draws background
   then sphere into a native-resolution PSRAM RGB565 buffer, disk sized by
   `scaleX`.
4. The buffer is copied into `pendingFullImageBuffer`, `imageReadyToDisplay` is
   set, and the existing swap-and-draw path shows it with identity pipeline
   scale and the per-source offset.

Interactive render (while dragging the displayed moon):

1. Touch travel crosses the drag threshold. `interactiveMoonMode` set, cycling
   paused.
2. Per frame: `moon_interaction` advances eased yaw/pitch; `moon_sphere_render`
   into the 240x240 persistent buffers; PPA upscales to full screen;
   `drawBitmap` pushes it.
3. On release and settle, clear `interactiveMoonMode`, resume cycling.

## Memory and performance

- Texture resident: 4 MB PSRAM (2048x1024 RGB565), allocated once.
- Resting render buffers: color plus Z at native resolution, about 1.3 MB at
  800x800, allocated per render and freed, or held if simpler.
- Drag buffers: color plus Z at 240x240, about 230 KB, persistent during drag.
- Flash: about 500 KB for the texture array plus tgx headers, against ~10 MB
  free in the app partition.
- Resting render time: a few hundred ms at 800x800 (acceptable for a slideshow
  refresh). Drag target: 15 to 20 fps via 240x240 render plus PPA upscale.

## Risks and verification

- tgx build for esp32p4 Arduino: header-heavy templates. Mitigation: a minimal
  compile check early in implementation before porting the full renderer.
- PSRAM budget: resolved by the 32 MB measurement above. No fallback needed.
- Touch gesture coexistence: the drag threshold must not break single-tap to
  advance or double-tap to toggle. Verify on device.
- Display type variants: covered by the display resolution handling section.
  The renderer reads `displayManager.getWidth()/getHeight()` rather than a fixed
  size, so both the 720x720 and 800x800 panels render correctly. Verify on
  whichever panel is connected, and ideally test a Display Hardware switch.

## Out of scope

- Changes to non-moon image sources, the download pipeline, or the scaling of
  HTTP-downloaded images.
- Moon rise and set times (present in NINA, not required here unless requested
  later).
