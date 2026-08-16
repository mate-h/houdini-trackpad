extern "C" {
#include "multitouch/multitouch-device.h"
}

#define _USE_MATH_DEFINES

#include <DM/DM_EventTable.h>
#include <DM/DM_MouseHook.h>
#include <DM/DM_RenderTable.h>
#include <DM/DM_SceneHook.h>
#include <DM/DM_VPortAgent.h>
#include <GUI/GUI_GeoRender.h>
#include <GUI/GUI_ViewState.h>
#include <HOM/HOM_BoundingBox.h>
#include <HOM/HOM_BoundingRect.h>
#include <HOM/HOM_GeometryViewport.h>
#include <HOM/HOM_Vector2.h>
#include <HOM/HOM_GeometryViewportCamera.h>
#include <HOM/HOM_Matrix3.h>
#include <HOM/HOM_Matrix4.h>
#include <HOM/HOM_Module.h>
#include <HOM/HOM_NetworkEditor.h>
#include <HOM/HOM_ReferencePlane.h>
#include <HOM/HOM_SceneViewer.h>
#include <HOM/HOM_ui.h>
#include <RE/RE_Render.h>
#include <RE/RE_Window.h>
#include <UI/UI_Event.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Map.h>
#include <UT/UT_VectorTypes.h>
#include <algorithm>
#include <cmath>
#include <dispatch/dispatch.h>

namespace HDK_Plugin {

// Helper function called on main thread to trigger viewport redraw
static void requestViewportRedrawCallback(void *context) {
  HOM_GeometryViewportCamera *camera =
      static_cast<HOM_GeometryViewportCamera *>(context);
  if (camera != NULL) {
    // Camera is available in context if needed for updates
    camera->setWindowSize(camera->windowSize());
  }
}

// Helper function to trigger viewport redraw on main thread
void requestViewportRedrawOnMainThread(HOM_GeometryViewportCamera *camera) {
  if (camera == NULL) {
    return;
  }
  // Use dispatch_async_f to post to main queue (main thread)
  // This is thread-safe and will execute requestViewportRedrawCallback on the
  // main thread
  dispatch_async_f(dispatch_get_main_queue(), camera,
                   requestViewportRedrawCallback);
}

// Consumes events if trackpad is present
//
// DM_TrackpadEventHook objects are created by DM_TrackpadHook.
const uint NO_GESTURE = 0;
const uint SCROLL_GESTURE = 1;
const uint PINCH_GESTURE = 2;
const uint ROTATE_GESTURE = 3;

class DM_TrackpadEventHook : public DM_MouseEventHook {
public:
  DM_TrackpadEventHook(DM_VPortAgent &vport)
      : DM_MouseEventHook(vport, DM_VIEWPORT_ALL_3D) {}
  virtual ~DM_TrackpadEventHook() {}

  const uint velScale = 10; // multiplier
  const uint zoomScale = 10;    // multiplier

  bool shiftDown = false;
  bool altDown = false;
  bool ctrlDown = false;
  bool cmdDown = false;

  // Deferred camera update to avoid re-entrancy crashes
  HOM_GeometryViewportCamera *deferredCamera = NULL;
  bool deferredPerspective = false;
  bool hasDeferredUpdate = false;

  void trackpadCallback(int currentGesture, fpreal zoom, fpreal vx, fpreal vy) {
    HOM_Module &hou = HOM();
    HOM_ui &ui = hou.ui();
    HOM_SceneViewer *scene =
        dynamic_cast<HOM_SceneViewer *>(ui.paneTabUnderCursor());

    // check if hovering scene and window is focused
    // TODO: bug - appActive only returns true after first "re-focus" of the
    // window
    if (scene == NULL) {
      return;
    }

    DM_VPortAgent &vport = viewport();
    DM_ViewportType viewType = vport.getViewType();
    GUI_ViewState &viewState = vport.getViewStateRef();
    HOM_GeometryViewport *currentViewport = scene->curViewport();

    // If viewport type is 0, try to determine if it's 3D by checking the
    // current viewport Viewports start as type 0 but become 3D when initialized
    bool is3DViewport = false;
    if (viewType == 0) {
      // Type 0 might be uninitialized - check if we have a valid geometry
      // viewport
      if (currentViewport != NULL) {
        std::string viewportTypeName = currentViewport->type().name();
        // Assume it's 3D if it's a known 3D viewport type
        if (viewportTypeName == "Perspective" || viewportTypeName == "Top" ||
            viewportTypeName == "Front" || viewportTypeName == "Right" ||
            viewportTypeName == "Left" || viewportTypeName == "Bottom" ||
            viewportTypeName == "Back") {
          is3DViewport = true;
        }
      }
    } else {
      // Viewport has a proper type - check if it's 3D
      is3DViewport = (viewType & DM_VIEWPORT_ALL_3D) != 0;
    }

    if (!is3DViewport) {
      return;
    }

    std::string currentViewportType = currentViewport->type().name();
    bool isCurrentViewport = false;
    if (currentViewportType == "Perspective" &&
        (viewType == DM_VIEWPORT_PERSPECTIVE || viewType == 0)) {
      isCurrentViewport = true;
    } else if (currentViewportType == "Top" &&
               (viewType == DM_VIEWPORT_TOP || viewType == 0)) {
      isCurrentViewport = true;
    } else if (currentViewportType == "Right" &&
               (viewType == DM_VIEWPORT_RIGHT || viewType == 0)) {
      isCurrentViewport = true;
    } else if (currentViewportType == "Front" &&
               (viewType == DM_VIEWPORT_FRONT || viewType == 0)) {
      isCurrentViewport = true;
    } else if (currentViewportType == "Left" &&
               (viewType == DM_VIEWPORT_LEFT || viewType == 0)) {
      isCurrentViewport = true;
    }
    // For type 0 viewports, accept any 3D viewport type
    else if (viewType == 0 && is3DViewport) {
      isCurrentViewport = true;
    }
    if (!isCurrentViewport) {
      return;
    }

    // Get camera to determine view type and save/restore view state
    HOM_GeometryViewportCamera *camera = currentViewport->defaultCamera();
    if (camera == NULL) {
      return;
    }

    // Determine if this is a perspective or orthographic view using camera
    bool isPerspective = camera->isPerspective();
    bool isOrtho = camera->isOrthographic();

    fpreal dx = velScale * vx;
    fpreal dy = velScale * vy;
    fpreal dzoom = zoomScale * zoom;
    fpreal scrollZoom = zoomScale * vy;
    bool zoomModifier = ctrlDown || cmdDown;

    // Apply view transformations
    if (isPerspective) {
      if (currentGesture == SCROLL_GESTURE) {
        if (zoomModifier) {
          viewState.dolly(scrollZoom);
        } else if (shiftDown) {
          viewState.scroll(dx, dy);
        } else {
          viewState.dotumble(dx, dy, 1);
        }
      } else if (currentGesture == PINCH_GESTURE) {
        viewState.dolly(dzoom);
      }
    } else if (isOrtho) {
      if (currentGesture == SCROLL_GESTURE) {
        if (zoomModifier) {
          viewState.zoom(scrollZoom,
                         GUI_ViewParameter::GUI_ZoomItem::GUI_ORTHO_WIDTH);
        } else {
          viewState.scroll(dx, dy);
        }
      } else if (currentGesture == PINCH_GESTURE) {
        viewState.zoom(dzoom, GUI_ViewParameter::GUI_ZoomItem::GUI_ORTHO_WIDTH);
      }
    }

    requestViewportRedrawOnMainThread(camera);
  }

  virtual bool handleMouseEvent(const DM_MouseHookData &hook_data,
                                UI_Event *event) {
    altDown = event->state.altFlags & UI_ALT_KEY;
    shiftDown = event->state.altFlags & UI_SHIFT_KEY;
    ctrlDown = event->state.altFlags & UI_CTRL_KEY;
    cmdDown = event->state.altFlags & UI_COMMAND_KEY;
    return false;
  }
  virtual bool handleMouseWheelEvent(const DM_MouseHookData &hook_data,
                                     UI_Event *event) {
    altDown = event->state.altFlags & UI_ALT_KEY;
    shiftDown = event->state.altFlags & UI_SHIFT_KEY;
    ctrlDown = event->state.altFlags & UI_CTRL_KEY;
    cmdDown = event->state.altFlags & UI_COMMAND_KEY;
    // consume scroll event
    return true;
  }
  virtual bool handleDoubleClickEvent(const DM_MouseHookData &hook_data,
                                      UI_Event *event) {
    return false;
  }

  virtual bool allowRMBMenu(const DM_MouseHookData &hook_data,
                            UI_Event *event) {
    return true;
  }

  int bumpRefCount(bool inc) {
    refCount += (inc ? 1 : -1);
    return refCount;
  }

private:
  int refCount;
};

UT_Map<int, DM_TrackpadEventHook *> mouseHooks;

HOM_NetworkEditor *getNetworkEditor() {
  HOM_Module &hou = HOM();
  HOM_ui &ui = hou.ui();
  HOM_NetworkEditor *networkEditor =
      dynamic_cast<HOM_NetworkEditor *>(ui.paneTabUnderCursor());
  return networkEditor;
}

struct NetworkViewUpdate {
  uint gesture;
  double tx;
  double ty;
  double zoom;
};

static void applyNetworkViewCallback(void *context) {
  NetworkViewUpdate *update = static_cast<NetworkViewUpdate *>(context);
  HOM_NetworkEditor *editor = getNetworkEditor();
  if (editor != NULL) {
    if (update->gesture == SCROLL_GESTURE) {
      editor->translate(HOM_Vector2(update->tx, update->ty));
    } else if (update->gesture == PINCH_GESTURE) {
      // Zoom about the cursor in network space so pan is already baked in
      // (same as hou's scaleAroundMouse).
      HOM_Vector2 pivot = editor->cursorPosition();
      HOM_BoundingRect bounds = editor->visibleBounds();
      const double scale = std::exp(-update->zoom * 0.02);
      if (scale > 1e-4 && std::isfinite(scale)) {
        bounds.translate({-pivot.x(), -pivot.y()});
        bounds.scale({scale, scale});
        bounds.translate({pivot.x(), pivot.y()});
        editor->setVisibleBounds(bounds, 0.0);
      }
    }
  }
  delete update;
}

void applyNetworkViewOnMainThread(uint gesture, double tx, double ty,
                                  double zoom) {
  NetworkViewUpdate *update = new NetworkViewUpdate{gesture, tx, ty, zoom};
  dispatch_async_f(dispatch_get_main_queue(), update, applyNetworkViewCallback);
}

// Classify pinch vs tumble from raw MultitouchSupport contacts.
// Lock the winner for the rest of the two-finger stroke so a noisy frame
// cannot flip the camera mode mid-gesture.
const float kMinFingerSize = 0.18;
const double kSessionTimeout = 0.12;
const fpreal kMinDecideMotion = 0.012;
const fpreal kPinchDominance = 1.3;
const fpreal kTumbleDominance = 1.4;
const int kMaxUndecidedFrames = 8;

struct GestureRecognizer {
  uint gesture = NO_GESTURE;
  bool havePrev = false;
  int idA = -1;
  int idB = -1;
  UT_Vector2D prevA;
  UT_Vector2D prevB;
  UT_Vector2D startMid;
  fpreal startDist = 0;
  fpreal prevDist = 0;
  fpreal tumbleEvidence = 0;
  fpreal pinchEvidence = 0;
  int frames = 0;
  double lastTs = 0;

  void reset() {
    gesture = NO_GESTURE;
    havePrev = false;
    idA = -1;
    idB = -1;
    startDist = 0;
    prevDist = 0;
    tumbleEvidence = 0;
    pinchEvidence = 0;
    frames = 0;
    lastTs = 0;
  }

  bool samePair(int ia, int ib) const {
    return (ia == idA && ib == idB) || (ia == idB && ib == idA);
  }

  bool update(const Finger &fa, const Finger &fb, double ts, fpreal &outZoom,
              fpreal &outVx, fpreal &outVy) {
    UT_Vector2D a(fa.normalized.pos.x, fa.normalized.pos.y);
    UT_Vector2D b(fb.normalized.pos.x, fb.normalized.pos.y);
    int ia = fa.identifier;
    int ib = fb.identifier;

    if (havePrev && (ts - lastTs > kSessionTimeout || !samePair(ia, ib))) {
      reset();
    }

    const fpreal dist = (a - b).length();
    const UT_Vector2D mid = (a + b) * 0.5;

    if (!havePrev) {
      idA = ia;
      idB = ib;
      prevA = a;
      prevB = b;
      startMid = mid;
      startDist = dist;
      prevDist = dist;
      lastTs = ts;
      havePrev = true;
      return false;
    }

    if (ia == idB && ib == idA) {
      std::swap(a, b);
      std::swap(ia, ib);
    }

    const UT_Vector2D dA = a - prevA;
    const UT_Vector2D dB = b - prevB;
    const fpreal speedA = dA.length();
    const fpreal speedB = dB.length();
    fpreal coherence = 0;
    if (speedA > 1e-6 && speedB > 1e-6) {
      coherence = dA.dot(dB) / (speedA * speedB);
    }

    const UT_Vector2D prevMid = (prevA + prevB) * 0.5;
    const fpreal midStep = (mid - prevMid).length();
    const fpreal distStep = std::abs(dist - prevDist);

    // Same-direction motion votes tumble; opposing / radial motion votes pinch.
    tumbleEvidence += midStep * (0.6 + 0.4 * std::max(coherence, 0.0));
    pinchEvidence += distStep * (0.6 + 0.4 * std::max(-coherence, 0.0));
    frames++;

    if (gesture == NO_GESTURE) {
      const fpreal midTravel = (mid - startMid).length();
      const fpreal distTravel = std::abs(dist - startDist);
      const bool enough = midTravel >= kMinDecideMotion ||
                          distTravel >= kMinDecideMotion ||
                          frames >= kMaxUndecidedFrames;
      if (enough) {
        if (pinchEvidence > tumbleEvidence * kPinchDominance &&
            distTravel >= kMinDecideMotion * 0.65) {
          gesture = PINCH_GESTURE;
        } else if (tumbleEvidence > pinchEvidence * kTumbleDominance) {
          gesture = SCROLL_GESTURE;
        } else if (frames >= kMaxUndecidedFrames) {
          gesture =
              (pinchEvidence > tumbleEvidence) ? PINCH_GESTURE : SCROLL_GESTURE;
        }
      }
    }

    outZoom = (dist - prevDist) * 100;
    outVx = (fa.normalized.vel.x + fb.normalized.vel.x) * 0.5;
    outVy = (fa.normalized.vel.y + fb.normalized.vel.y) * 0.5;

    prevA = a;
    prevB = b;
    prevDist = dist;
    lastTs = ts;
    return gesture != NO_GESTURE;
  }
};

GestureRecognizer gRecognizer;

int trackpadCallback(int device, Finger *data, int nFingers, double timestamp,
                     int frame) {
  RE_Render *render = RE_Render::getMainRender();
  if (nFingers != 2) {
    gRecognizer.reset();
    return 0;
  }
  bool windowActive = render->getCurrentWindow()->appActive();
  if (!windowActive) {
    gRecognizer.reset();
    return 0;
  }
  Finger *fa = &data[0];
  Finger *fb = &data[1];
  if (fa->size < kMinFingerSize || fb->size < kMinFingerSize) {
    return 0;
  }

  fpreal zoom = 0;
  fpreal dx = 0;
  fpreal dy = 0;
  if (!gRecognizer.update(*fa, *fb, timestamp, zoom, dx, dy)) {
    return 0;
  }
  const uint currentGesture = gRecognizer.gesture;

  HOM_NetworkEditor *networkEditor = getNetworkEditor();
  if (networkEditor != NULL) {
    HOM_BoundingRect bounds = networkEditor->visibleBounds();
    double bx = bounds.size().x();
    double by = bounds.size().y();
    const float scrollScale = 0.02;
    applyNetworkViewOnMainThread(
        currentGesture, -dx * bx * scrollScale, -dy * by * scrollScale, zoom);
    return 0;
  }

  if (mouseHooks.size() == 0) {
    return 0;
  }
  for (auto it = mouseHooks.begin(); it != mouseHooks.end(); ++it) {
    it->second->trackpadCallback(currentGesture, zoom, dx, dy);
  }
  return 0;
}

// DM_TrackpadHook is a factory for DM_TrackpadEventHook objects.
class DM_TrackpadHook : public DM_MouseHook {
public:
  DM_TrackpadHook() : DM_MouseHook("Trackpad", 0) {
    fprintf(stderr, "[TrackpadHook] DM_TrackpadHook constructor called\n");
    fflush(stderr);
  }

  // Called 4 times at startup
  virtual DM_MouseEventHook *newEventHook(DM_VPortAgent &vport) {
    int viewType = vport.getViewType();
    // Create hooks for all viewports initially - they might be type 0 before
    // initialization The hook itself will filter events based on viewport type
    // Only skip if we're sure it's not a 3D viewport (type 0 might become 3D
    // later)
    if (viewType != 0 && !(viewType & DM_VIEWPORT_ALL_3D)) {
      return NULL;
    }

    DM_TrackpadEventHook *hook = NULL;

    // Create only 1 per viewport
    const int id = vport.getUniqueId();
    UT_Map<int, DM_TrackpadEventHook *>::iterator it = mouseHooks.find(id);

    if (it != mouseHooks.end()) {
      // found existing hook for this viewport, reuse it.
      hook = it->second;
    } else {
      // no hook for this viewport; create it.
      hook = new DM_TrackpadEventHook(vport);
      mouseHooks[id] = hook;
    }

    // increase reference count on the render so we know when to delete
    // it.
    hook->bumpRefCount(true);

    return hook;
  }

  virtual void retireEventHook(DM_VPortAgent &vport, DM_MouseEventHook *hook) {
    // If the ref count is zero, we're the last retire call. delete the
    // hook.
    if (static_cast<DM_TrackpadEventHook *>(hook)->bumpRefCount(false) == 0) {
      // Remove from the map and delete the hook.
      const int id = vport.getUniqueId();
      UT_Map<int, DM_TrackpadEventHook *>::iterator it = mouseHooks.find(id);

      if (it != mouseHooks.end())
        mouseHooks.erase(id);

      delete hook;
    }
  }
};

} // namespace HDK_Plugin

using namespace HDK_Plugin;

void DMnewEventHook(DM_EventTable *table) {
  table->registerMouseHook(new DM_TrackpadHook);
  startDevice(trackpadCallback);
}
