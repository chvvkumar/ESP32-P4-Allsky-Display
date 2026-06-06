# Image Sources: Split Sun, Moon, and GOES Selectors Into Rows

Date: 2026-06-06
Status: Approved for planning

## Goal

Rework the "Image Sources" card on the web config images page (`/config/images`)
so the preset selectors are split into three dedicated rows (Sun, Moon Phase,
GOES) and the moon source presents its phase settings both in the add toolbar
and in the source list.

## Scope

Frontend only. All changes live in `HTML_IMAGES_APP` inside `web_config_html.h`
(functions `renderSourcesCard`, `renderRow`, `bind`, `bindRows`, plus new shared
moon-control helpers).

No backend changes. The `/api/images/state` endpoint already returns:
- `presets`: static catalog including `sdo_*`, `soho_*`, `goes19_full`, `__moon__`.
- `moon`: a single global moon config object (lat, lon, bg, flipu, flipv, roll,
  yaw, pitch, northup, light, spin, spinret).
- `sources[]`: each with an `isMoon` boolean (true when url starts with `moon://`).

Existing endpoints reused without modification:
- `POST /api/addPreset` with `id` (`__moon__` adds the computed moon source).
- `POST /api/setMoon` with the moon config payload.

## Current State

`renderSourcesCard` produces:
1. A generic URL add bar (text input plus Add).
2. A single preset dropdown mixing all Sun, GOES, and Moon presets, plus one Add.
3. A collapsible "Moon (computed) settings" box with the full moon settings grid.

In the source list, the moon row shows `moon://default` in a read-only text
input and exposes the generic transform drawer (scale, offset, rotation).

## Target Layout

### Add toolbar (top of the Image Sources card)

1. Generic URL bar: retained unchanged (text input plus Add). This is the path
   for arbitrary AllSky image URLs.
2. Sun row: `Sun` label, dropdown of solar presets (ids starting with `sdo_` or
   `soho_`), Add button.
3. Moon Phase row: `Moon Phase` label, inline `Background` select, inline
   `North up` select, a settings cog button, and an Add button.
   - The cog opens a drawer containing the remaining moon settings: latitude,
     longitude, flip horizontal, flip vertical, roll, yaw, pitch, drag-light
     mode, spin mode, free-spin return seconds, and a Save moon button.
   - The Add button calls `/api/addPreset` with `__moon__`. It is disabled when
     a moon source already exists in the list, to prevent duplicates.
4. GOES row: `GOES` label, dropdown of GOES presets (ids starting with `goes`),
   Add button.

Presets are partitioned client-side by id prefix from the existing `presets`
array. No new preset ids are introduced.

### Source list moon row

For the source flagged `isMoon`:
- Replace the read-only `moon://default` URL input with a static `Moon Phase`
  label. The raw url string is not shown and not editable.
- Replace the generic transform caret and drawer with the same shared moon
  controls used by the Moon Phase add row: inline Background select, inline
  North up select, and a settings cog that opens the full moon settings drawer.
- Retain the standard list affordances: enable/disable toggle, index number,
  duration field, bulk-select checkbox, and delete.

Non-moon rows are unchanged.

## Shared Moon Controls

Extract a single pair of helpers so the Moon Phase add row and the in-list moon
row render and bind identical controls:
- `renderMoonControls(opts)`: returns the inline Background and North up selects,
  the cog button, and the collapsible settings drawer markup. Element ids are
  namespaced (for example by a caller-supplied prefix) so the same markup can
  appear twice on the page without id collisions.
- `bindMoonControls(opts)`: wires the cog toggle, the inline selects, and the
  Save moon button to `/api/setMoon`, then `refetch()`.

Both instances read from and write to the single global `state.moon`, so they
stay in sync by construction. A change made in one location is reflected in the
other after `refetch()`.

## Data Flow

- Background, North up, and cog-drawer inputs in either location submit to
  `/api/setMoon` with the same payload shape as the current `saveMoonBtn`
  handler, followed by `refetch()` to reload `state.moon`.
- Sun, GOES, and Moon Add buttons submit the selected preset id to
  `/api/addPreset`, then `refetch()`.
- Error handling and toasts mirror the existing handlers (`inputErr`, `toast`,
  network-error fallbacks).

## Out of Scope

- No changes to the playback card, default transform card, or save bar.
- No changes to C++ handlers, the preset catalog, or NVS storage.
- No new moon settings or fields beyond those already present.

## Acceptance Criteria

1. The Image Sources add toolbar shows three distinct preset rows: Sun, Moon
   Phase, and GOES, each with its own Add button.
2. The Sun dropdown lists only solar presets; the GOES dropdown lists only GOES
   presets; neither contains the moon entry.
3. The Moon Phase row shows inline Background and North up controls and a cog
   that reveals the remaining moon settings.
4. The Moon Phase Add button adds the computed moon source and is disabled while
   a moon source already exists.
5. In the source list, the moon row shows the label `Moon Phase` instead of
   `moon://default` and exposes the same moon controls (Background, North up,
   cog) rather than the generic transform drawer.
6. Editing moon settings in either the add row or the list row updates the same
   global moon config and both reflect the change after reload.
7. Non-moon rows and the generic URL add bar are unchanged.
8. The sketch compiles without errors.
