/** \file primaryRayTrace/main.cpp */
#include <G3D/G3D.h>

class App : public GApp {
private:
    enum ProjectionAlgorithm {PLANAR = 0, SPHERICAL = 1, LENSLET = 2};
    
    ProjectionAlgorithm                      m_projectionAlgorithm = PLANAR;
    shared_ptr<Framebuffer>                  m_rayFramebuffer;
    shared_ptr<TriTree>                      m_triTree;

    // Best perforance with wave.lib is achieved with 2
    // sets of output buffers
    int                                      m_frameIdx = 0;
    shared_ptr<GLPixelTransferBuffer>        m_rayOriginSSBO;
    shared_ptr<GLPixelTransferBuffer>        m_rayDirectionSSBO;
    Array<shared_ptr<GLPixelTransferBuffer>> m_outBuffers[2];

    void allocateSSBO(shared_ptr<GLPixelTransferBuffer>& ssbo, int width, int height, int bindpoint) {
        ssbo = GLPixelTransferBuffer::create(width, height, ImageFormat::RGBA32F());
        ssbo->bindAsShaderStorageBuffer(bindpoint);
    }

public:
    App(const GApp::Settings& settings) : GApp(settings) { m_triTree = TriTree::create(true); }
    virtual void onInit() override;
    virtual void onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& surface3D) override;
    virtual void onAfterLoadScene(const Any& any, const String& sceneName) override;
};


void App::onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& surface3D) {
    // Rebuild the BVH when out of date
    if (m_triTree->lastBuildTime() < scene()->lastVisibleChangeTime()) {
        m_triTree->setContents(scene());
    }

    // Disable TAA
    activeCamera()->filmSettings().setTemporalAntialiasingEnabled(false);

    //////////////////////////////////////////////////////////////////////////////////
    // (re)Allocate primary ray buffers
    if (isNull(m_rayOriginSSBO) || m_rayOriginSSBO->width() != rd->window()->width() || m_rayOriginSSBO->height() != rd->window()->height()) {
        allocateSSBO(m_rayOriginSSBO, rd->window()->width(), rd->window()->height(), 0);
        allocateSSBO(m_rayDirectionSSBO, rd->window()->width(), rd->window()->height(), 1);
    }

    debugAssertGLOk();

    BEGIN_PROFILER_EVENT("Ray Generation");
    const Rect2D viewport = Rect2D::xywh(0.0f, 0.0f, (float)m_rayOriginSSBO->width(), (float)m_rayOriginSSBO->height());
    {

        Args args;
        args.setRect(viewport);


        const float blockCols = 16.0f;
        const float blockRows = 16.0f;
        activeCamera()->setShaderArgs(args, viewport.wh(), "camera.");
        args.setMacro("PROJECTION_ALGORITHM", m_projectionAlgorithm);
        // Must set this variable when launching a compute shader
        args.setComputeGridDim(Vector3int32(iCeil(viewport.width() / blockCols), iCeil(viewport.height() / blockRows), 1));
        // In the current API, this variable is optional.
        args.setComputeGroupSize(Vector3int32((int)blockCols, (int)blockRows, 1));
        //args.setUniform("screenWidth", m_rayOriginSSBO->width());
        debugAssertGLOk();
        LAUNCH_SHADER("generateRays.glc", args);
    }
    END_PROFILER_EVENT();

    debugAssertGLOk();
    //////////////////////////////////////////////////////////////////////////////////
    // Cast primary rays, storing results in a non-coherent GBuffer
    m_gbuffer->setSpecification(m_gbufferSpecification);
    m_gbuffer->resize(m_rayOriginSSBO->width(), m_rayOriginSSBO->height());
    m_gbuffer->prepare(rd, activeCamera(), 0, -(float)previousSimTimeStep(), Vector2int16(), Vector2int16());

    if (m_outBuffers[m_frameIdx].size() == 0 || 
        m_outBuffers[m_frameIdx][0]->width() != m_rayOriginSSBO->width() ||
        m_outBuffers[m_frameIdx][0]->height() != m_rayOriginSSBO->height()) {
        m_outBuffers[m_frameIdx].resize(5);

        const int width = m_rayOriginSSBO->width(), height = m_rayDirectionSSBO->height();
        for (int i = 0; i < 5; ++i) {
            switch (i) {
            case 2:
            case 3:
                m_outBuffers[m_frameIdx][i] = GLPixelTransferBuffer::create(width, height, ImageFormat::RGBA8());// , nullptr, 1, GL_STREAM_DRAW);
                break;
            default:
                m_outBuffers[m_frameIdx][i] = GLPixelTransferBuffer::create(width, height, ImageFormat::RGBA32F());// , nullptr, 1, GL_STREAM_DRAW);
            }
        }
    }

    // OptiXTriTree fast path. These buffers are directly mapped in wave.lib.
    m_triTree->intersectRays(m_rayOriginSSBO, m_rayDirectionSSBO, m_outBuffers[m_frameIdx].getCArray());

    m_gbuffer->texture(GBuffer::Field::WS_POSITION)->update(m_outBuffers[m_frameIdx][0]);
    m_gbuffer->texture(GBuffer::Field::WS_NORMAL)->update(m_outBuffers[m_frameIdx][1]);
    m_gbuffer->texture(GBuffer::Field::LAMBERTIAN)->update(m_outBuffers[m_frameIdx][2]);
    m_gbuffer->texture(GBuffer::Field::GLOSSY)->update(m_outBuffers[m_frameIdx][3]);
    m_gbuffer->texture(GBuffer::Field::EMISSIVE)->update(m_outBuffers[m_frameIdx][4]);
    
    //TODO: restore
    //m_frameIdx = 1 - m_frameIdx;

    //////////////////////////////////////////////////////////////////////////////////
    // Compute shadow maps
    Light::renderShadowMaps(rd, scene()->lightingEnvironment().lightArray, surface3D);

    //////////////////////////////////////////////////////////////////////////////////
    // Perform deferred shading on the GBuffer
    rd->push2D(m_framebuffer); {
        // Disable screen-space effects
        LightingEnvironment e = scene()->lightingEnvironment();

        Args args;
        e.ambientOcclusionSettings.enabled = false;
        e.setShaderArgs(args);
        args.setMacro("COMPUTE_PERCENT", 0);
        m_gbuffer->setShaderArgsRead(args, "gbuffer_");
        args.setRect(rd->viewport());

        LAUNCH_SHADER("DefaultRenderer/DefaultRenderer_deferredShade.pix", args);
    } rd->pop2D();

    swapBuffers();
    rd->clear();

    // Disable all positional effects
    FilmSettings postSettings = activeCamera()->filmSettings();
    postSettings.setAntialiasingEnabled(true);
    postSettings.setTemporalAntialiasingEnabled(false);
    postSettings.setVignetteBottomStrength(0);
    postSettings.setVignetteTopStrength(0);
    postSettings.setBloomStrength(0);
    m_film->exposeAndRender(rd, postSettings, m_framebuffer->texture(0), settings().hdrFramebuffer.trimBandThickness().x, settings().hdrFramebuffer.depthGuardBandThickness.x);
}


void App::onAfterLoadScene(const Any& any, const String& sceneName) {
    GApp::onAfterLoadScene(any, sceneName);
    // Disable TAA and postFX
    FilmSettings& postSettings = activeCamera()->filmSettings();
    postSettings.setTemporalAntialiasingEnabled(false);
    postSettings.setAntialiasingEnabled(false);
    postSettings.setTemporalAntialiasingEnabled(false);
    postSettings.setVignetteBottomStrength(0);
    postSettings.setVignetteTopStrength(0);
    postSettings.setBloomStrength(0);}


void App::onInit() {
    GApp::onInit();         
    //developerWindow->setVisible(true);
    developerWindow->cameraControlWindow->setVisible(false);
    showRenderingStats = false;

    setFrameDuration(1.0f / 1000.0f, -200.0f);

    m_gbufferSpecification.encoding[GBuffer::Field::WS_POSITION].format = ImageFormat::RGBA32F();
    m_gbufferSpecification.encoding[GBuffer::Field::WS_NORMAL]          = Texture::Encoding(ImageFormat::RGBA16F(), FrameName::CAMERA, 1.0f, 0.0f);
    m_gbufferSpecification.encoding[GBuffer::Field::EMISSIVE].format    = ImageFormat::RGBA16F();
    m_gbufferSpecification.encoding[GBuffer::Field::GLOSSY].format      = ImageFormat::RGBA8();
    m_gbufferSpecification.encoding[GBuffer::Field::LAMBERTIAN].format = ImageFormat::RGBA8();
    m_gbufferSpecification.encoding[GBuffer::Field::TRANSMISSIVE].format  = ImageFormat::RGB16F();

    // Removing the depth buffer forces the deferred shader to read the explicit position buffer
    m_gbufferSpecification.encoding[GBuffer::Field::DEPTH_AND_STENCIL].format = nullptr;
    m_gbufferSpecification.encoding[GBuffer::Field::CS_NORMAL] = nullptr;

    loadScene(
#       ifndef G3D_DEBUG
            "G3D Sponza"
#       else
            "G3D Simple Cornell Box (Area Light)"
#       endif
    );
}


G3D_START_AT_MAIN();

int main(int argc, const char* argv[]) {
    initGLG3D();

    GApp::Settings settings(argc, argv);  
    settings.window.caption = "Primary Ray Tracing";
    settings.window.width = OSWindow::primaryDisplayWindowSize().x;
    settings.window.height =  OSWindow::primaryDisplayWindowSize().y;
    settings.window.refreshRate = -1;
    settings.window.asynchronous = true;
    //settings.window.resizable = true;
    settings.hdrFramebuffer.colorGuardBandThickness = settings.hdrFramebuffer.depthGuardBandThickness  = Vector2int16(0, 0);

    return App(settings).run();
}
