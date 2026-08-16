# houdini-trackpad

![preview](https://raw.githubusercontent.com/mate-h/houdini-trackpad/master/static/houdini-trackpad.webp)

This plugin enables multitouch gestures in Houdini's viewport for Macbooks running OSX.
Controls:

- Tumble using two fingers
- Pan with Shift + two-finger scroll
- Zoom with Ctrl/Cmd + two-finger scroll, or pinch
- In the network editor, two-finger scroll pans (no Shift) and pinch zooms

[Download v0.3](https://github.com/mate-h/houdini-trackpad/releases/download/0.3/TrackpadHook.dylib)

Place the `TrackpadHook.dylib` binary in `~/Library/Preferences/houdini/[version]/dso` in order to install.

## Build from source

```bash
mkdir -p build
cd build
cmake ../src
make
```

CMake finds Houdini at `/Applications/Houdini/Current`. `make` installs `TrackpadHook.dylib` into your Houdini DSO folder (e.g. `~/Library/Preferences/houdini/22.0/dso/`). Restart Houdini after building.

To target a specific install, set `HFS` to that build's Resources directory before running cmake:

```bash
export HFS=/Applications/Houdini/Houdini22.0.368/Frameworks/Houdini.framework/Versions/Current/Resources
```

## Run test program with python

```bash
python3 -m pip install -U pygame numpy --user
python3 src/scripts/multitouch-test.py
```
