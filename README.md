### Copyright

PaintHD  
Copyright (c) 2026 WojciechGw  
based on paint.c example  
Copyright (c) 2025 Rumbledethumps

SPDX-License-Identifier: BSD-3-Clause

# PaintHD

PaintHD is a monochrome bitmap paint program for the Picocomputer RP6502 target.
It works on a `640 x 480` canvas in `1 bpp` mode and stores images as monochrome
BMP files. The project also includes `paintHDloader`, a small launcher that
builds file thumbnails and starts PaintHD with a selected image.

## Main Applications

### `paintHD`

The editor itself. It provides direct mouse-based drawing, a movable toolbar
picker, selection, clipboard operations, primitive shapes, save/restore, and
multi-step undo/redo stored on disk.

### `paintHDloader`

A launcher for browsing BMP files in the program directory. It shows up to
`16` thumbnails per page, supports paging with `PgUp` / `PgDn`, and launches
`painthd.rp6502` with the selected file. Empty slots can be used to start a new
image using the default name `paintHD_new.bmp`.

## PaintHD Overview

PaintHD is designed for fast editing of black-and-white images with a compact
toolbar placed directly on screen. The editor uses:

- left and right mouse buttons as two paint colors
- a swap button to exchange the color assigned to each mouse button
- a picker bar for tools, brush settings, coordinates, and status
- temporary disk files for clipboard and undo/redo history

The canvas is always monochrome:

- pixel value `0` = black
- pixel value `1` = white

## Available Functions

### Freehand Drawing

- Paint with the left mouse button using the color assigned to the left button.
- Paint with the right mouse button using the color assigned to the right button.
- Brush size can be changed from `1` to `64`.
- Available brush shapes:
  - square
  - circle
  - vertical line
  - diagonal line
  - spray
  - fill

### Brush Variants

- Double-click the vertical brush icon to switch between vertical and horizontal.
- Double-click the diagonal brush icon to switch between left and right diagonal.
- Single click on the brush size field sets size to `1 px`.
- Double-click on the brush size field sets size to `5 px`.
- Triple-click on the brush size field sets size to the maximum size.

### Straight Lines

- `Ctrl + LMB` starts or continues straight line drawing.
- `Ctrl + Shift + LMB` constrains the line to horizontal or vertical directions.

### Primitive Shapes

Two dedicated tools are available:

- rectangle / square
- ellipse / circle

Behavior:

- click the primitive tool icon
- press and drag on the canvas to size the primitive
- release the mouse button to draw the final shape
- hold the left `Ctrl` key while sizing to force equal width and height

The current brush shape is used to draw the primitive outline, except for
`fill`, which is disabled for primitive mode.

### Selection

Rectangular selection is supported.

- Select the selection tool to enter selection mode.
- Drag with `LMB` to create a rectangular selection.
- The current selection size is shown live in the status field while dragging.
- After the selection is finished, its border remains visible.
- Selection mode stays active until explicitly cleared.

When a selection exists:

- painting is clipped to the selected rectangle
- wipe, invert, and mirror can work on the selection instead of the whole canvas
- clipboard operations use the selected area

### Clipboard

Clipboard data is stored in a temporary disk file.

Supported operations:

- copy selection
- cut selection
- opaque paste
- transparent paste

Paste preview follows the mouse before confirmation.

### Canvas Operations

- wipe full canvas
- wipe selection
- invert full canvas
- invert selection
- mirror full canvas
- mirror selection

The mirror tool has two modes:

- vertical mirror
- horizontal mirror

Double-clicking the mirror icon switches between them.

### Save / Restore

- Save the current image to BMP.
- Auto-restore `painthd_save.bmp` can be performed on startup when no explicit
  file is provided.
- If PaintHD is started with a BMP file path, saving writes back to that file.

### Undo / Redo

- Multi-step undo and redo are stored on disk.
- New editing operations clear the redo history.
- Undo/redo restore both canvas content and the dirty state.

### Long Operation Cancellation

Long-running operations support `Esc` cancellation, including:

- fill
- wipe
- invert
- mirror
- clipboard copy / cut / paste
- save / load paths that use chunked processing

## Keyboard Shortcuts

### General

- `Esc`  
  Cancels active preview/selection mode or interrupts long operations that
  support cancellation.

### Drawing

- `Ctrl + LMB`  
  Draw straight line segments.

- `Ctrl + Shift + LMB`  
  Constrain line drawing to horizontal or vertical.

### Selection and Clipboard

- `Ctrl + A`  
  Clear the current selection and return to normal brush work area.

- `Ctrl + C`  
  Copy the current selection to the clipboard file.

- `Ctrl + X`  
  Cut the current selection to the clipboard file.

- `Ctrl + V`  
  Start opaque paste preview.

- `Ctrl + Alt + V`  
  Start transparent paste preview.

### History

- `Ctrl + Z`  
  Undo.

- `Ctrl + Y`  
  Redo.

### Program Exit

- `Alt + F10`  
  Save to `painthd_save.bmp` and exit.

## Mouse Controls

- `LMB`  
  Paint with left-button color, confirm paste, choose tools, create selection,
  size primitives.

- `RMB`  
  Paint with right-button color, cancel paste preview.

- mouse move  
  Move pointer, update paste preview, update primitive preview, update selection
  preview.

## Picker Description

The picker is a movable on-screen toolbar rendered as its own bitmap plane.

### Picker Elements

From left to right, the picker contains:

1. move handle
2. title: `PaintHD`
3. save
4. color swap
5. wipe
6. invert
7. mirror
8. selection tool
9. rectangle tool
10. ellipse tool
11. brush tool icons
12. brush size `-`
13. brush size value
14. brush size `+`
15. mouse coordinates
16. status field

### Picker Behavior

- The handle allows the toolbar to be dragged.
- The active tool is highlighted.
- Disabled tools are dimmed.
- Hover text appears in the status field when no higher-priority status is active.
- Operation results such as `SAVED`, `RESTORED`, `cancelled`, or selection size
  messages are shown in the status field.

### Tool Interaction Rules

- `fill` disables rectangle and ellipse tools.
- rectangle and ellipse disable `fill`.
- mirror and invert adapt their meaning to selection state.
- wipe also adapts to selection state.

## paintHDloader Description

`paintHDloader` is a separate launcher program.

### Purpose

- scan the program directory for BMP files
- build monochrome thumbnails
- show files as a `4 x 4` grid
- launch PaintHD with the selected file

### Behavior

- up to `16` files are shown per page
- empty slots are filled with a checker pattern
- thumbnails are cached as ready-made `1 bpp` page buffers for faster return to
  already visited pages

### Loader Controls

- `LMB` on an occupied tile  
  Launch PaintHD with that BMP file.

- `LMB` on an empty tile  
  Launch PaintHD with `paintHD_new.bmp`.

- `PgUp`  
  Previous page, if available.

- `PgDn`  
  Next page, if available.

## Disk Directories and Runtime Files

### Runtime Directories

- `MSC0:/paintHD/`  
  Application root directory.

- `MSC0:/paintHD/TMP/`  
  Temporary working directory used by PaintHD and the loader.

### Runtime Temporary Files

- `MSC0:/paintHD/TMP/painthd_clip.bin`  
  Clipboard file.

- `MSC0:/paintHD/TMP/paintHD_c.bin`  
  Current snapshot used by undo/redo staging.

- `MSC0:/paintHD/TMP/paintHD_u###.bin`  
  Undo snapshots.

- `MSC0:/paintHD/TMP/paintHD_r###.bin`  
  Redo snapshots.

- `MSC0:/paintHD/TMP/loader_p###.bin`  
  Cached loader pages with ready-made `1 bpp` thumbnails.

### Main Image Files

- `paintHD_new.bmp`  
  Default file name for a new image.

- `painthd_save.bmp`  
  Default save/restore file used by startup restore and forced exit save.

- `BAK_<name>.bmp`  
  Backup created when a file is explicitly loaded for editing.

## Repository Layout

- `src/`  
  C source and header files for PaintHD, PaintHD Loader, icons, and BMP helpers.

- `assets/`  
  Binary assets, generated data, and text documentation such as:
  - `assets/painthd.txt`
  - `assets/paintHDloader.txt`

- `tools/`  
  Build helper scripts and RP6502-related tooling.

- `build/`  
  Generated build output such as executables and linker maps.

- `.vscode/`  
  Editor configuration for VSCode / IntelliSense.

## Notes

- PaintHD is tuned for `cc65` and the RP6502 environment.
- The canvas uses `1 bpp`, so many tools are optimized around monochrome data.
- Temporary disk files are an intentional part of the design to avoid pressure
  on limited RAM/BSS space.

