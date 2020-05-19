from __future__ import with_statement

# {{{ MultitouchSupport
import time
import ctypes
import threading
from ctypes.util import find_library

CFArrayRef = ctypes.c_void_p
CFMutableArrayRef = ctypes.c_void_p
CFIndex = ctypes.c_long

MultitouchSupport = ctypes.CDLL("/System/Library/PrivateFrameworks/MultitouchSupport.framework/MultitouchSupport")

CFArrayGetCount = MultitouchSupport.CFArrayGetCount
CFArrayGetCount.argtypes = [CFArrayRef]
CFArrayGetCount.restype = CFIndex

CFArrayGetValueAtIndex = MultitouchSupport.CFArrayGetValueAtIndex
CFArrayGetValueAtIndex.argtypes = [CFArrayRef, CFIndex]
CFArrayGetValueAtIndex.restype = ctypes.c_void_p

MTDeviceCreateList = MultitouchSupport.MTDeviceCreateList
MTDeviceCreateList.argtypes = []
MTDeviceCreateList.restype = CFMutableArrayRef

class MTPoint(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float),
                ("y", ctypes.c_float)]

class MTVector(ctypes.Structure):
    _fields_ = [("position", MTPoint),
                ("velocity", MTPoint)]

class MTData(ctypes.Structure):
    _fields_ = [
      ("frame", ctypes.c_int),
      ("timestamp", ctypes.c_double),
      ("identifier", ctypes.c_int),
      ("state", ctypes.c_int),  # Current state (of unknown meaning).
      ("unknown1", ctypes.c_int),
      ("unknown2", ctypes.c_int),
      ("normalized", MTVector),  # Normalized position and vector of
                                 # the touch (0 to 1).
      ("size", ctypes.c_float),  # The area of the touch.
      ("unknown3", ctypes.c_int),
      # The following three define the ellipsoid of a finger.
      ("angle", ctypes.c_float),
      ("major_axis", ctypes.c_float),
      ("minor_axis", ctypes.c_float),
      ("unknown4", MTVector),
      ("unknown5_1", ctypes.c_int),
      ("unknown5_2", ctypes.c_int),
      ("unknown6", ctypes.c_float),
    ]

MTDataRef = ctypes.POINTER(MTData)

MTContactCallbackFunction = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int, MTDataRef,
    ctypes.c_int, ctypes.c_double, ctypes.c_int)

MTDeviceRef = ctypes.c_void_p

MTRegisterContactFrameCallback = MultitouchSupport.MTRegisterContactFrameCallback
MTRegisterContactFrameCallback.argtypes = [MTDeviceRef, MTContactCallbackFunction]
MTRegisterContactFrameCallback.restype = None

MTDeviceStart = MultitouchSupport.MTDeviceStart
MTDeviceStart.argtypes = [MTDeviceRef, ctypes.c_int]
MTDeviceStart.restype = None

MTDeviceStop = MultitouchSupport.MTDeviceStop
MTDeviceStop.argtypes = [MTDeviceRef]
#MTDeviceStop.restype = None

def _cfarray_to_list(arr):
    rv = []
    n = CFArrayGetCount(arr)
    for i in range(n):
        rv.append(CFArrayGetValueAtIndex(arr, i))
    return rv

# }}}

import json
import sys
from queue import Queue

touches_lock = threading.Lock()
touches = []

def init_multitouch(cb):
    devices = _cfarray_to_list(MultitouchSupport.MTDeviceCreateList())
    for device in devices:
        MTRegisterContactFrameCallback(device, cb)
        MTDeviceStart(device, 0)
    return devices

def stop_multitouch(devices):
    for device in devices:
        MTDeviceStop(device)

@MTContactCallbackFunction
def touch_callback(device, data_ptr, n_fingers, timestamp, frame):
    fingers = []
    for i in range(n_fingers):
        fingers.append(data_ptr[i])
    touches[:] = [(frame, timestamp, fingers)]
    return 0

import pygame
from pygame import draw, display, mouse
from pygame.locals import *
from numpy import *

pygame.init()

#n_samples = 22050 * 4
#sa = zeros((n_samples, 2))
#sound = sndarray.make_sound(sa)
#sa = sndarray.samples(sound)
#sound.play(-1)

devs = init_multitouch(touch_callback)

#flags = HWSURFACE | DOUBLEBUF
#mode = max(display.list_modes(0, flags))
#display.set_mode(mode, flags)
display.set_mode((640, 480))
screen = display.get_surface()
width, height = screen.get_size()

mouse.set_visible(False)

fingers = []

while True:
    for event in pygame.event.get():
        if event.type == pygame.QUIT: sys.exit()

    if touches:
        frame, timestamp, fingers = touches.pop()

    #print frame, timestamp
    screen.fill((0xef, 0xef, 0xef))

    try:
        prev = None
        for i, finger in enumerate(fingers):
            pos = finger.normalized.position
            vel = finger.normalized.velocity

            x = int(pos.x * width)
            y = int((1 - pos.y) * height)
            p = (x, y)
            r = int(finger.size * 10)
            #print "finger", i, "at", (x, y)
            #xofs = int(finger.minor_axis / 2)
            #yofs = int(finger.major_axis / 2)

            if prev:
                draw.line(screen, (0xd0, 0xd0, 0xd0), p, prev[0], 3)
                draw.circle(screen, 0, prev[0], prev[1], 0)
            prev = p, r

            draw.circle(screen, 0, p, r, 0)
            #draw.ellipse(screen, 0, (x - xofs, y - yofs, xofs * 2, yofs * 2))

            #sa[int(pos.x * n_samples)] = int(-32768 + pos.y * 65536)

            vx = vel.x
            vy = -vel.y
            posvx = x + vx / 10 * width
            posvy = y + vy / 10 * height
            draw.line(screen, 0, p, (posvx, posvy))

        # EXIT! One finger still, four motioning quickly downward.
        if len(fingers) == 5:
            n_still = 0
            n_down = 0
            for i, finger in enumerate(fingers):
                vel = finger.normalized.velocity
                #print i, "%.2f, %.2f" % (vel.x, vel.y)
                t = 0.1
                if -t <= vel.x < t and -t <= vel.y < t:
                    n_still += 1
                elif -2 <= vel.x < 2 and vel.y < -4:
                    n_down += 1
            if n_still == 1 and n_down == 4:
                break
    except:
        print("An exception occurred")

    display.flip()

stop_multitouch(devs)