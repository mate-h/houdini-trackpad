extern "C" {
	#include "trackpad/trackpad.h"
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

class DM_TrackpadEventHook : public DM_MouseEventHook
{
public:
    DM_TrackpadEventHook(DM_VPortAgent &vport)
	: DM_MouseEventHook(vport, DM_VIEWPORT_ALL_3D)
		{ }
    virtual ~DM_TrackpadEventHook() { }

		const uint velScale = 10; // multiplier
		const uint slowVelScale = 1;
		const uint zoomScale = 1000; // multiplier
		const uint slowZoomScale = 100; // multiplier
		const uint NO_GESTURE = 0;
		const uint SCROLL_GESTURE = 1;
		const uint ZOOM_GESTURE = 2;
		const uint threshold = 50; // ms
		const float sizeThreshold = 0.3;
		const float zoomThreshold = 10;
		const float zoomThresholdN = -3;

		uint currentGesture = NO_GESTURE;
		uint prevGesture = NO_GESTURE;
		bool shiftDown = false;
		bool altDown = false;
		bool ctrlDown = false;
		fpreal prevdistance;
		high_resolution_clock::time_point prevTime;

		void trackpadCallback(int device, Finger *data, int nFingers, double timestamp, int frame) {
			// this hook supports two finger gestures
			if (nFingers != 2) {
				return;
			}

			HOM_Module &hou = HOM();
			RE_Render* render = RE_Render::getMainRender();
			HOM_ui &ui = hou.ui();
			HOM_PaneTab* hoveringTab = ui.paneTabUnderCursor();
			HOM_PaneTab* tab = (ui.paneTabOfType(HOM_paneTabType::SceneViewer));
			HOM_SceneViewer* scene = dynamic_cast<HOM_SceneViewer*>(tab);
			HOM_SceneViewer* hoveringScene = dynamic_cast<HOM_SceneViewer*>(hoveringTab);

			// check if hovering scene and window is focused
			// TODO: bug - appActive only returns true after first "re-focus" of the window
			if (hoveringScene == NULL || !render->getCurrentWindow()->appActive()) {
				return;
			}

			DM_VPortAgent &vport = viewport();
			DM_ViewportType viewType = vport.getViewType();
			GUI_ViewState &viewState = vport.getViewStateRef();
			
			Finger *fa = &data[0];
			Finger *fb = &data[1];
			if (fa->size < sizeThreshold || fb->size < sizeThreshold) {
				return;
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
			fpreal zoom = (shiftDown ? slowZoomScale : zoomScale) * (distance - prevdistance);
			prevdistance = distance;

			if (elapsed > threshold) {
				currentGesture = NO_GESTURE;
				viewState.setTumbling(false);
			}
			else if (((zoom > 0 && zoom < zoomThreshold) || (zoom < 0 && zoom > zoomThresholdN)) && currentGesture == NO_GESTURE) {
				currentGesture = SCROLL_GESTURE;
				viewState.setTumbling(true);
			}
			else if (currentGesture == NO_GESTURE) {
				currentGesture = ZOOM_GESTURE;
				viewState.setTumbling(true);
			}

			if (prevGesture != currentGesture) {
				// std::cout << vport.getUniqueId() << " " << currentGesture << "\n";
			}
			prevGesture = currentGesture;
			
			// calculate average velocity of the two fingers together
			fpreal vavgx = (shiftDown ? slowVelScale : velScale) * (fa->normalized.vel.x + fb->normalized.vel.x) / 2;
			fpreal vavgy = (shiftDown ? slowVelScale : velScale) * (fa->normalized.vel.y + fb->normalized.vel.y) / 2;
			
			
			// if (hoveringScene != NULL && render->getCurrentWindow()->appActive()) {
			if (viewType == DM_VIEWPORT_PERSPECTIVE || viewType == DM_VIEWPORT_ORTHO) {
				if (currentGesture == SCROLL_GESTURE) {
					viewState.dotumble(vavgx, vavgy, 1);
					// scene->curViewport()->frameBoundingBox();
					//scene->curViewport()->draw();
					// 
				}
				else if (currentGesture == ZOOM_GESTURE) {
					viewState.dolly(zoom);
				}
			}
			else {
				if (currentGesture == SCROLL_GESTURE)
					viewState.scroll(vavgx, vavgy);
				else if (currentGesture == ZOOM_GESTURE)
					viewState.zoom(zoom, GUI_ViewParameter::GUI_ZoomItem::GUI_ORTHO_WIDTH);
			}
			HOM_BoundingBox box;
			const uint size = 10;
			box.setTo({-size, -size, -size, size, size, size});
			scene->curViewport()->frameBoundingBox(&box);
			vport.refresh();
			
			// }
			/* 
			identifier
				Persistent identifier for each touch -- each "finger" may move around the Fingers[] array, but this will remain the same.
				
			normalized.pos.x
				Current position, from [0..1]
				
			size
				Close to zero if you're barely touching the touch pad
				
			angle, majorAxis, minorAxis
				Describes the ellipsoid of your finger. Yes, you can track rotation of a single finger!

			printf("Frame %7d: Angle %6.2f, ellipse %6.3f x%6.3f; "
				"position (%6.3f,%6.3f) vel (%6.3f,%6.3f) "
				"ID %d, state %d [%d %d?] size %6.3f, %6.3f?\n",
				f->frame,
				f->angle * 90 / atan2(1,0),
				f->majorAxis,
				f->minorAxis,
				f->normalized.pos.x,
				f->normalized.pos.y,
				f->normalized.vel.x,
				f->normalized.vel.y,
				f->identifier, f->state, f->foo3, f->foo4,
				f->size, f->unk2); */
		// printf("\n");
		
		}
    
    virtual bool	handleMouseEvent(const DM_MouseHookData &hook_data,
					 UI_Event *event)
	{
			altDown = event->state.altFlags & UI_ALT_KEY;
			shiftDown = event->state.altFlags & UI_SHIFT_KEY;
			ctrlDown = event->state.altFlags & UI_CTRL_KEY;
	    return true;
	}
    virtual bool	handleMouseWheelEvent(const DM_MouseHookData &hook_data,
					      UI_Event *event)
	{		
			altDown = event->state.altFlags & UI_ALT_KEY;
			shiftDown = event->state.altFlags & UI_SHIFT_KEY;
			ctrlDown = event->state.altFlags & UI_CTRL_KEY;

			// event->state.values[Z]
			// GUI_ViewState &state = viewport().getViewStateRef();
			// state.dolly(-event->state.values[Z]*0.30);
			// state.set
			// std::cout << state.getTransformMatrix()[3][2] << "\n";
			//viewport().refresh();
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

	int		bumpRefCount(bool inc)
	{
			refCount += (inc?1:-1);
			return refCount;
	}

private:
	int	refCount;
};

class DM_DummyEventHook : public DM_MouseEventHook
{
	public:
	DM_DummyEventHook(DM_VPortAgent &vport)
	: DM_MouseEventHook(vport, DM_VIEWPORT_ALL_3D) { }
    virtual ~DM_DummyEventHook() { }
	virtual bool	handleMouseEvent(const DM_MouseHookData &hook_data,
					 UI_Event *event)
	{
	    return false;
	}
    virtual bool	handleMouseWheelEvent(const DM_MouseHookData &hook_data,
					      UI_Event *event)
	{		
			// std::cout << event->type == UI_EventType::UI_EVENT_MOUSEWHEEL;
			GUI_ViewState &state = viewport().getViewStateRef();
			state.dolly(event->state.values[Z]*0.30);
			viewport().requestDraw();
	    return true;
	}
    virtual bool	handleDoubleClickEvent(
					    const DM_MouseHookData &hook_data,
					    UI_Event *event)
	{
	    return false;
	}
};

UT_Map<int, DM_TrackpadEventHook *> mouseHooks;
int trackpadCallback(int device, Finger *data, int nFingers, double timestamp, int frame) {
	UT_Map<int,DM_TrackpadEventHook *>::iterator it;
	for(it = mouseHooks.begin(); 
			it != mouseHooks.end(); ++it) {
		it->second->trackpadCallback(device, data, nFingers, timestamp, frame);
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
