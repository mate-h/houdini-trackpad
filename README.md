# houdini-trackpad

![preview](https://github.com/mateh/houdini-trackpad/blob/master/static/houdini-trackpad.webp?raw=true)

This plugin enables multitouch gestures in Houdini's viewport for Macbooks running OSX.
Controls:

- Tumble/scroll using two fingers
- Dolly/zoom using pinch gesture
- Pan by holding Alt and two finger scroll
- Rotate using rotate gesture
- Hold Shift with any gesture to slow down movement

## Build from source

```bash
export VERSION = 17.5.229 # change this to your version of Houdini installation
source /Applications/Houdini/Houdini$VERSION/Frameworks/Houdini.framework/Versions/Current/Resources/houdini_setup
git clone https://github.com/mate-h/houdini-trackpad
cd houdini-trackpad/build
cmake ../src
make
```

## Run test program with python

```bash
python3 -m pip install -U pygame==2.0.0.dev6 --user
python3 -m pip install -U numpy --user
python3 src/scripts/multitouch-test.py
```
