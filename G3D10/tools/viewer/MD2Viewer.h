/**
  \file tools/viewer/MD2Viewer.h

  G3D Innovation Engine https://casual-effects.com/g3d
  Copyright 2000-2020, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/
#pragma once
#define MD2Viewer_h

#include "G3D/G3D.h"
#include "Viewer.h"

class MD2Viewer : public Viewer {
private:
    shared_ptr<MD2Model>               m_model;
    Array<shared_ptr<Surface>>         m_surfaceArray;
    MD2Model::Pose                     m_pose;
    MD2Model::Pose::Action             m_action;

    void pose(RealTime deltaTime);

public:
    virtual void onInit(const String& filename) override;
    virtual void onGraphics3D(RenderDevice* rd, App* app, const shared_ptr<LightingEnvironment>& lighting, Array<shared_ptr<Surface> >& surfaceArray) override;
};
