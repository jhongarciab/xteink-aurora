# Xteink Aurora UI Preview

Lightweight browser preview for arranging firmware screens without compiling or
flashing the reader after every visual adjustment. The Home preview follows the
current Lyra Carousel firmware layout. Reading Stats, Time Sync, and Settings
follow their corresponding C++ activity layouts and Lyra theme metrics. It
models the portrait X3 and X4 screen proportions, selection state, and a 1-bit
dark-mode approximation.

Open `index.html` directly in a browser, or serve the repository root locally:

```sh
python3 -m http.server 8080
```

Then visit:

```text
http://localhost:8080/tools/ui-preview/
```

The preview saves its current controls in browser local storage. **Save layout**
downloads the selected configuration as JSON so a proposed arrangement can be
shared or referenced while implementing the corresponding C++ activity.

This is a layout tool, not a firmware emulator. Exact font metrics, framebuffer
rendering, button behavior, e-ink refreshes, and memory use still require a
firmware build and final verification on the device.

## USB screenshots

Development firmware accepts `CMD:SCREENSHOT` over serial. The response includes
the runtime framebuffer width and height, so `scripts/debugging_monitor.py` can
now create correctly sized portrait screenshots for both X3 and X4 devices.
