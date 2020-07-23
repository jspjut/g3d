/**
  \file tools/viewer/Viewer.h

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2019, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/
#ifndef Viewer_h
#define Viewer_h

#include "G3D/G3D.h"
#include "App.h"

class Viewer {
public:
    virtual ~Viewer() {}
    virtual void onInit(const G3D::String& filename) = 0;
    virtual bool onEvent(const GEvent& e, App* app) { return false; }
    virtual void onSimulation(RealTime rdt, SimTime sdt, SimTime idt) {}
    virtual void onGraphics3D(RenderDevice* rd, App* app, const shared_ptr<LightingEnvironment>& lighting, Array<shared_ptr<Surface> >& surfaceArray) {};
    virtual void onGraphics2D(RenderDevice* rd, App* app) {}
    virtual void onPose(Array<shared_ptr<Surface> >& posed3D, Array<shared_ptr<Surface2D> >& posed2D) {};

};

#endif 
