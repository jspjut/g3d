/**
  \file tools/viewer/MD2Viewer.cpp

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2019, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/
#include "MD2Viewer.h"


void MD2Viewer::pose(RealTime dt){
    m_pose.onSimulation(SimTime(dt), m_action);

    const CoordinateFrame& cframe = CFrame::fromXYZYPRDegrees(0, 0, 0, 180);
    
    m_surfaceArray.fastClear();
    m_model->pose(m_surfaceArray, cframe, cframe, nullptr, &m_pose, &m_pose, Surface::ExpressiveLightScatteringProperties());
}


void MD2Viewer::onInit(const String& filename) {
    m_model = MD2Model::create(filename);
    m_pose = MD2Model::Pose(MD2Model::STAND, 0);
}


void MD2Viewer::onGraphics3D(RenderDevice* rd, App* app, const shared_ptr<LightingEnvironment>& lighting, Array<shared_ptr<Surface> >& surfaceArray) {
    app->colorClear = Color3::white();
    screenPrintf("Triangles: %d", m_model->numTriangles());
    screenPrintf("Animation: %d (number keys to change)", m_pose.animation);

    {
        const UserInput* ui = app->userInput;
        m_action.attack = ui->keyDown(GKey('1'));
        m_action.crouching = ui->keyDown(GKey('2'));
        m_action.movingForward = ui->keyDown(GKey('3'));
        m_action.point = ui->keyDown(GKey('4'));
        m_action.flip = ui->keyDown(GKey('5'));
        m_action.fallback = ui->keyDown(GKey('6'));
        m_action.death1 = ui->keyDown(GKey('7'));
        m_action.salute = ui->keyDown(GKey('8'));
        m_action.wave = ui->keyDown(GKey('9'));
        m_action.pain1 = ui->keyDown(GKey('0'));
    }
    
    pose(app->previousSimTimeStep());
    
    LightingEnvironment env;
    env.lightArray = lighting->lightArray;
    
    for (const shared_ptr<Surface>& surface : m_surfaceArray) {
        surface->render(rd, env, RenderPassType::OPAQUE_SAMPLES);
    }
}
