/**
  \file tools/viewer/MD3Viewer.h

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2019, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/
#ifndef MD3Viewer_h
#define MD3Viewer_h

#include "G3D/G3D.h"
#include "Viewer.h"

class MD3Viewer : public Viewer {
private:
    shared_ptr<MD3Model>               model;
    MD3Model::Pose                     currentPose;
    shared_ptr<Texture>                m_skybox;
    CoordinateFrame                    cframe;

    void pose(RealTime deltaTime);

public:

    virtual void onInit(const String& filename) override;
    virtual void onPose(Array<shared_ptr<Surface> >& posed3D, Array<shared_ptr<Surface2D> >& posed2D) override;
    virtual void onGraphics3D(RenderDevice* rd, App* app, const shared_ptr<LightingEnvironment>& lighting, Array<shared_ptr<Surface> >& surfaceArray) override;
};

#endif 
