# houdini-trackpad

![preview](https://raw.githubusercontent.com/mate-h/houdini-trackpad/master/static/houdini-trackpad.webp)

This plugin enables multitouch gestures in Houdini's viewport for Macbooks running OSX.
Controls:

- Tumble/scroll using two fingers
- Dolly/zoom using pinch gesture
- Pan by holding Alt and two finger scroll
- Hold Shift with any gesture to slow down movement

In `v0.2`:

- Rotate using rotate gesture

[Download v0.1 pre-release](https://github.com/mate-h/houdini-trackpad/releases/download/0.1/TrackpadHook.dylib)

Place the `TrackpadHook.dylib` binary in `~/Library/Preferences/houdini/[version]/dso` in order to install.

<a href="https://www.buymeacoffee.com/mateh" target="_blank"><img src="https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png" alt="Buy Me A Coffee" style="height: 41px !important;width: 174px !important;box-shadow: 0px 3px 2px 0px rgba(190, 190, 190, 0.5) !important;-webkit-box-shadow: 0px 3px 2px 0px rgba(190, 190, 190, 0.5) !important;" ></a>

<form action="https://www.paypal.com/cgi-bin/webscr" method="post" target="_top">
<input type="hidden" name="cmd" value="_donations" />
<input type="hidden" name="business" value="4YHXGYE9P9QJG" />
<input type="hidden" name="item_name" value="Coffee Drinking" />
<input type="hidden" name="currency_code" value="USD" />
<input type="image" src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif" border="0" name="submit" title="PayPal - The safer, easier way to pay online!" alt="Donate with PayPal button" />
<img alt="" border="0" src="https://www.paypal.com/en_US/i/scr/pixel.gif" width="1" height="1" />
</form>

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
