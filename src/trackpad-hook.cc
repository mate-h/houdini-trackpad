extern "C" {
	#include "multitouch/multitouch-device.h"
}

#define _USE_MATH_DEFINES

#include <UT/UT_DSOVersion.h>
#include <UT/UT_Map.h>
#include <DM/DM_EventTable.h>
#include <DM/DM_MouseHook.h>
#include <DM/DM_VPortAgent.h>
#include <DM/DM_RenderTable.h>
#include <DM/DM_SceneHook.h>
#include <UI/UI_Event.h>
#include <GUI/GUI_ViewState.h>
#include <UT/UT_VectorTypes.h>
#include <HOM/HOM_Module.h>
#include <HOM/HOM_ui.h>
#include <HOM/HOM_SceneViewer.h>
#include <HOM/HOM_NetworkEditor.h>
#include <HOM/HOM_ReferencePlane.h>
#include <HOM/HOM_GeometryViewport.h>
#include <HOM/HOM_Matrix4.h>
#include <RE/RE_Render.h>
#include <RE/RE_Window.h>
#include <cmath>
#include <chrono>

using namespace std::chrono;

namespace HDK_Plugin {

// Consumes events if trackpad is present
//
// DM_TrackpadEventHook objects are created by DM_TrackpadHook.
const uint NO_GESTURE = 0;
const uint SCROLL_GESTURE = 1;
const uint PINCH_GESTURE = 2;
const uint ROTATE_GESTURE = 2;

class DM_TrackpadEventHook : public DM_MouseEventHook
{
public:
    DM_TrackpadEventHook(DM_VPortAgent &vport)
	: DM_MouseEventHook(vport, DM_VIEWPORT_ALL_3D)
		{ }
    virtual ~DM_TrackpadEventHook() { }

		const uint velScale = 10; // multiplier
		const uint slowVelScale = 1;
		const uint zoomScale = 10; // multiplier
		const uint slowZoomScale = 1; // multiplier
		
		bool shiftDown = false;
		bool altDown = false;
		bool ctrlDown = false;

		void trackpadCallback(int currentGesture, fpreal zoom, fpreal vx, fpreal vy) {
			HOM_Module &hou = HOM();
			HOM_ui &ui = hou.ui();
			HOM_SceneViewer* scene = dynamic_cast<HOM_SceneViewer*>(ui.paneTabUnderCursor());

			// check if hovering scene and window is focused
			// TODO: bug - appActive only returns true after first "re-focus" of the window
			if (scene == NULL) {
				return;
			}

			DM_VPortAgent &vport = viewport();
			DM_ViewportType viewType = vport.getViewType();
			GUI_ViewState &viewState = vport.getViewStateRef();
			HOM_GeometryViewport* currentViewport = scene->curViewport();
			std::string currentViewportType = currentViewport->type().name();
			bool isCurrentViewport = false;
			if (currentViewportType == "Perspective" && viewType == DM_VIEWPORT_PERSPECTIVE) {
				isCurrentViewport = true;
			}
			else if (currentViewportType == "Top" && viewType == DM_VIEWPORT_TOP) {
				isCurrentViewport = true;
			}
			else if (currentViewportType == "Right" && viewType == DM_VIEWPORT_RIGHT) {
				isCurrentViewport = true;
			}
			else if (currentViewportType == "Front" && viewType == DM_VIEWPORT_FRONT) {
				isCurrentViewport = true;
			}
			else if (currentViewportType == "Left" && viewType == DM_VIEWPORT_LEFT) {
				isCurrentViewport = true;
			}
			if (!isCurrentViewport) {
				return;
			}
			
			// calculate average velocity of the two fingers together
			fpreal dx = (shiftDown ? slowVelScale : velScale) * vx;
			fpreal dy = (shiftDown ? slowVelScale : velScale) * vy;
			fpreal dzoom = (shiftDown ? slowZoomScale : zoomScale) * zoom;

			// if (hoveringScene != NULL && render->getCurrentWindow()->appActive()) {
			if (viewType == DM_VIEWPORT_PERSPECTIVE || viewType == DM_VIEWPORT_ORTHO) {
				if (currentGesture == SCROLL_GESTURE) {
					if (ctrlDown) {
						viewState.scroll(dx, dy);
					}
					else viewState.dotumble(dx, dy, 1);
				}
				else if (currentGesture == PINCH_GESTURE) {
					viewState.dolly(dzoom);
				}
			}
			else {
				if (currentGesture == SCROLL_GESTURE)
					viewState.scroll(dx, dy);
				else if (currentGesture == PINCH_GESTURE)
					viewState.zoom(dzoom, GUI_ViewParameter::GUI_ZoomItem::GUI_ORTHO_WIDTH);
			}
			HOM_BoundingBox box;
			const uint size = 1000;
			box.setTo({-size, -size, -size, size, size, size});
			currentViewport->frameBoundingBox(&box);
			currentViewport->draw();
			// vport.refresh();
		}
    
    virtual bool	handleMouseEvent(const DM_MouseHookData &hook_data,
					 UI_Event *event)
	{
		altDown = event->state.altFlags & UI_ALT_KEY;
		shiftDown = event->state.altFlags & UI_SHIFT_KEY;
		ctrlDown = event->state.altFlags & UI_CTRL_KEY;
	  return false;
	}
    virtual bool	handleMouseWheelEvent(const DM_MouseHookData &hook_data,
					      UI_Event *event)
	{		
		altDown = event->state.altFlags & UI_ALT_KEY;
		shiftDown = event->state.altFlags & UI_SHIFT_KEY;
		ctrlDown = event->state.altFlags & UI_CTRL_KEY;
		// consume scroll event
		return true;
	}
    virtual bool	handleDoubleClickEvent(
					    const DM_MouseHookData &hook_data,
					    UI_Event *event)
	{
		return false;
	}

  virtual bool allowRMBMenu(const DM_MouseHookData &hook_data, UI_Event *event)
	{
		return true;
	}

	int	bumpRefCount(bool inc)
	{
			refCount += (inc?1:-1);
			return refCount;
	}

private:
	int	refCount;
};

UT_Map<int, DM_TrackpadEventHook *> mouseHooks;
UT_Vector2 lastVelocity;
HOM_NetworkEditor* getNetworkEditor() {
	HOM_Module &hou = HOM();
	HOM_ui &ui = hou.ui();
	HOM_NetworkEditor* networkEditor = dynamic_cast<HOM_NetworkEditor*>(ui.paneTabUnderCursor());
	return networkEditor;
}

const uint threshold = 50; // ms
const float sizeThreshold = 0.3;
const float zoomThreshold = 1;
const float zoomThresholdN = -0.3;

high_resolution_clock::time_point prevTime;
uint currentGesture = NO_GESTURE;
uint prevGesture = NO_GESTURE;
fpreal prevdistance;
int trackpadCallback(int device, Finger *data, int nFingers, double timestamp, int frame) {
	RE_Render* render = RE_Render::getMainRender();
	// this hook supports two finger gestures
	if (nFingers != 2 || !render->getCurrentWindow()->appActive()) {
		return 0;
	}
	Finger *fa = &data[0];
	Finger *fb = &data[1];
	if (fa->size < sizeThreshold || fb->size < sizeThreshold) {
		return 0;
	}
	// update timestamps
	high_resolution_clock::time_point time = high_resolution_clock::now();
	long long elapsed = duration_cast<milliseconds>(time - prevTime).count();
	prevTime = time;
	
	// calculate zoom factor using distance between fingers
	UT_Vector2D fap, fbp;
	fap.assign(fa->normalized.pos.x, fa->normalized.pos.y);
	fbp.assign(fb->normalized.pos.x, fb->normalized.pos.y);
	fpreal distance =  sqrt(pow(fap.x() - fbp.x(), 2) + pow(fap.y() - fbp.y(), 2));
	if (elapsed > threshold) {
		prevdistance = distance;
	}
	fpreal zoom = (distance - prevdistance) * 100;
	prevdistance = distance;

	if (elapsed > threshold) {
		currentGesture = NO_GESTURE;
	}
	else if (((zoom > 0 && zoom < zoomThreshold) || (zoom < 0 && zoom > zoomThresholdN)) && currentGesture == NO_GESTURE) {
		currentGesture = SCROLL_GESTURE;
	}
	else if (currentGesture == NO_GESTURE) {
		currentGesture = PINCH_GESTURE;
	}

	prevGesture = currentGesture;

	fpreal dx = (fa->normalized.vel.x + fb->normalized.vel.x) / 2;
	fpreal dy = (fa->normalized.vel.y + fb->normalized.vel.y) / 2;

	HOM_NetworkEditor* networkEditor = getNetworkEditor();
	if (networkEditor != NULL) {
		// calculate average velocity of the two fingers together
		auto bounds = networkEditor->visibleBounds();
		double bx = bounds.size().x();
		double by = bounds.size().y();
		const float scrollScale = 0.02;
		const float zoomScale =  0.01;
		if (currentGesture == SCROLL_GESTURE) {
			bounds.translate({-dx * bx * scrollScale, -dy * by * scrollScale});
		}
		else if (currentGesture == PINCH_GESTURE) {
			bounds.expand({-zoom * bx * zoomScale, -zoom * by * zoomScale});
		}
		
		networkEditor->setVisibleBounds(bounds);
		networkEditor->redraw();
		return 0;
	}

	UT_Map<int,DM_TrackpadEventHook *>::iterator it;
	for(it = mouseHooks.begin(); 
			it != mouseHooks.end(); ++it) {
		it->second->trackpadCallback(currentGesture, zoom, dx, dy);
	}
	return 0;
}

// DM_TrackpadHook is a factory for DM_TrackpadEventHook objects.
class DM_TrackpadHook : public DM_MouseHook
{
	public: DM_TrackpadHook() : DM_MouseHook("Trackpad", 0)
	{}

	// Called 4 times at startup
  virtual DM_MouseEventHook *newEventHook(DM_VPortAgent &vport)
	{
		// Only for 3D views (persp, top, bottom; not the UV viewport)
	  if(!(vport.getViewType() & DM_VIEWPORT_ALL_3D))
			return NULL;
	    
		DM_TrackpadEventHook *hook = NULL;

		// Create only 1 per viewport
		const int id = vport.getUniqueId();
		UT_Map<int,DM_TrackpadEventHook *>::iterator it=mouseHooks.find(id);

		if(it != mouseHooks.end())
		{
			// found existing hook for this viewport, reuse it.
			hook = it->second;
		}
		else
		{
			// no hook for this viewport; create it.
			hook = new DM_TrackpadEventHook(vport);
			mouseHooks[id] = hook;
		}

		// increase reference count on the render so we know when to delete
		// it.
		hook->bumpRefCount(true);
	    
		return hook;
	}

  virtual void retireEventHook(DM_VPortAgent &vport, DM_MouseEventHook *hook)
	{
		// If the ref count is zero, we're the last retire call. delete the
		// hook.
		if(static_cast<DM_TrackpadEventHook *>(hook)->bumpRefCount(false) == 0)
		{
			// Remove from the map and delete the hook.
			const int id = vport.getUniqueId();
			UT_Map<int,DM_TrackpadEventHook *>::iterator it
					= mouseHooks.find(id);
			
			if(it != mouseHooks.end())
					mouseHooks.erase(id);
				
			delete hook;
		}
	}
};

} // END HDK_Plugin namespace

using namespace HDK_Plugin;

void DMnewEventHook(DM_EventTable *table)
{
	table->registerMouseHook(new DM_TrackpadHook);
	startDevice(trackpadCallback);
}
