# Photo Viewer

A lightweight, portable image viewer for Windows written in C using only standard Windows APIs and the [stb_image](https://github.com/nothings/stb) single-header library.

**Developer:** [zrnge.com](https://zrnge.com)  
**GitHub:** [zrnge/photo-viewer](https://github.com/zrnge/photo-viewer)

## Features

- Supports **JPEG, PNG, BMP, TGA, PSD, GIF, HDR, PIC, PNM, PPM, PGM**
- Zoom in/out with mouse wheel or `+`/`-` buttons
- Click-and-drag panning with scroll bars
- Aspect-ratio-preserving fit-to-window
- "Open With" integration (right-click an image in Explorer)
- No external dependencies — single `.exe` file

## Build

```bash
gcc main.c -o PhotoViewer.exe -lgdi32 -lcomdlg32 -luser32 -Wl,-subsystem,windows
```

Requires `stb_image.h` from [nothings/stb](https://github.com/nothings/stb).

## Usage

- Run `PhotoViewer.exe` and click **Open Image**, or
- Right-click any image in Windows Explorer → **Open with** → choose `PhotoViewer.exe`
