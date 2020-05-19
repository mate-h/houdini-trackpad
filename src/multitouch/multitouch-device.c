// Code from https://web.archive.org/web/20151012175118/http://steike.com/code/multitouch/
// Combined with https://gist.github.com/lericson/236799

#include <math.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include "multitouch-device.h"

MTDeviceRef MTDeviceCreateDefault();
void MTRegisterContactFrameCallback(MTDeviceRef, MTContactCallbackFunction);
void MTDeviceStart(MTDeviceRef, int); // thanks comex
void MTDeviceStop(MTDeviceRef);

MTDeviceRef dev;
int startDevice(MTContactCallbackFunction callback) {
  dev = MTDeviceCreateDefault();
  MTRegisterContactFrameCallback(dev, callback);
  MTDeviceStart(dev, 0);
  return 0;
}

void stopDevice() {
  MTDeviceStop(dev);
}