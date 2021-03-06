/**
  @file App.h

  This is a simple ray tracing demo showing how to use the G3D ray tracing 
  primitives.  It runs fast enough for real-time flythrough of 
  a 100k scene at low resolution. At a loss of simplicity, it could be made
  substantially faster using adaptive refinement and multithreading.
 */
#ifndef App_h
#define App_h

#include <G3D/G3D.h>
#include <thread>

class World;

class App : public GApp {
private:
    
    int                 m_maxBounces;
    int                 m_raysPerPixel;

    bool                m_debugColoredSky = false;
    bool                m_debugNormals    = false;
    bool                m_debugMirrorDirection = false;

    bool                m_showReticle     = 
#       ifdef G3D_DEBUG
            true;
#       else
            false;
#       endif

    World*              m_world;
    
    /** Allocated by expose and render */
    shared_ptr<Texture> m_result;

    /** Used to pass information from rayTraceImage() to trace() */
    shared_ptr<Image3>  m_currentImage;

    /** Used to pass information from rayTraceImage() to trace() */
    int                 m_currentRays;

    /** Position during the previous frame */
    CFrame              m_prevCFrame;

    bool                m_forceRender;

    /** Called from onInit() */
    void makeGUI();

    /** Trace a single ray backwards. */
    Radiance3 rayTrace(const Ray& ray, World* world, Random& rng, int bounces = 1);

    /** Trace a whole image. */
    void rayTraceImage(float scale, int numRays);

    /** Show a full-screen message */
    void message(const String& msg) const;

    /** Trace one pixel of m_currentImage. Called on multiple threads. */
    void trace(int x, int y, Random& rng);

public:

    App(const GApp::Settings& settings = GApp::Settings());

    virtual void onInit() override;

    /** Callback for the render button */
    void onRender();

    virtual void onGraphics(RenderDevice* rd, Array<shared_ptr<Surface> >& posed3D, Array<shared_ptr<Surface2D> >& posed2D) override;
    virtual void onCleanup() override;

    virtual void onAfterLoadScene(const Any& any, const String& sceneName) override;
};

#endif
