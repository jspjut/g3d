/**
  \file G3D-app.lib/include/G3D-app/DefaultRenderer.h

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2019, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/
#pragma once
#define GLG3D_DefaultRenderer_h

#include "G3D-base/platform.h"
#include "G3D-app/Renderer.h"

namespace G3D {

class Camera;
class Texture;
class Args;
class Shader;

/** \brief Supports both traditional forward shading and full-screen deferred shading.

    The basic rendering algorithm is:

\code
Renderer::render(all) {
    visible, requireForward, requireBlended = cullAndSort(all)
    renderGBuffer(visible)
    computeShadowing(all)
    if (deferredShading()) { renderDeferredShading()  }
    renderOpaqueSamples(deferredShading() ? requireForward : visible)
    lighting.updateColorImage() // For the next frame
    renderOpaqueScreenSpaceRefractingSamples(deferredShading() ? requireForward : visible)
    renderBlendedSamples(requireBlended, transparencyMode)
}
\endcode

    The DefaultRenderer::renderDeferredShading() pass uses whatever properties are available in the
    GBuffer, which are controlled by the GBufferSpecification. For most applications,
    it is necessary to enable the lambertian, glossy, camera-space normal,
    and emissive fields to produce good results. If the current GBuffer specification
    does not contain sufficient fields, most of the surfaces will take the fallback
    forward shading pass at reduced performance.

    \sa GApp::m_renderer, G3D::RenderDevice, G3D::Surface
*/
class DefaultRenderer : public Renderer {
protected:
    /** e.g., "DefaultRenderer" used for switching the shaders loaded by subclasses. */
    String                      m_shaderName;

    /** e.g., "G3D::DefaultRenderer::" used for switching the shaders loaded by subclasses. */
    String                      m_textureNamePrefix;

    bool                        m_deferredShading;
    bool                        m_orderIndependentTransparency;

    /**
        Hi-res pixels per low-res pixel, along one dimension.
        (1 is identical resolution, 4 would be quarter-res,
        which is 1/16 the number of pixels).

        Default is 4.

        Set to 1 to disable low resolution OIT.
    */
    int                         m_oitLowResDownsampleFactor;

    /** Default is 1 */
    int                         m_oitUpsampleFilterRadius;

    /** If true, all OIT buffers will be in 32-bit floating point.
    
        Default is false. */
    bool                        m_oitHighPrecision;

    /** For the transparent surface pass of the OIT algorithm.
        Shares the depth buffer with the main framebuffer. The
        subsequent compositing pass uses the regular framebuffer in 2D mode. 
        
        This framebuffer has several color render targets bound. For details, see:

        McGuire and Mara, A Phenomenological Scattering Model for Order-Independent Transparency, I3D'16
        http://graphics.cs.williams.edu/papers/TransparencyI3D16/

        It shares the depth with the original framebuffer but does not write to it.
       */
    shared_ptr<Framebuffer>     m_oitFramebuffer;
    
    /** A low resolution version of m_oitFramebuffer. */
    shared_ptr<Framebuffer>     m_oitLowResFramebuffer;

    /** Used for resampling normals during computeLowResDepthAndNormals for later upsampling under OIT. Has a single
        RG8_SNORM texture that is camera-space octahedrally encoded normals. */
    shared_ptr<Framebuffer>     m_csOctLowResNormalFramebuffer;

    /** Captured image of the background used for blurs for OIT */ 
    shared_ptr<Framebuffer>     m_backgroundFramebuffer;
    
    /** Because subclasses can change the shader filename prefix, we must use
        member variables instead of the static variables created by LAUNCH_SHADER
        to store the shaders. These are loaded just before use. */
    shared_ptr<Shader>          m_deferredShader;
    shared_ptr<Shader>          m_upsampleOITShader;
    shared_ptr<Shader>          m_compositeOITShader;

    /** Loaded by the constructor, but subclasses may replace it in their own constructors.

        The default implementation is Weighted-Blended Order Independent Transparency by
        McGuire and Bavoil. This string can be overwritten to implement alternative algorithms,
        such as Adaptive Transparency. However, new buffers may need to be set by overriding
        renderOrderIndependentBlendedSamples() for certain algorithms.
    */
    String                      m_oitWriteDeclarationGLSLFilename;

    virtual void renderDeferredShading
       (RenderDevice*                       rd, 
        const Array<shared_ptr<Surface> >&  sortedVisibleSurfaceArray, 
        const shared_ptr<GBuffer>&          gbuffer,
        const LightingEnvironment&          environment);

    /** Subclasses that can compute global illumination to deferred shading buffers should override this method,
        which is invoked before renderDeferredShading. */
    virtual void renderIndirectIllumination
       (RenderDevice*                       rd,
        const Array<shared_ptr<Surface> >&  sortedVisibleSurfaceArray, 
        const shared_ptr<GBuffer>&          gbuffer, 
        const LightingEnvironment&          environment) {
        debugAssertM(m_deferredShading, "Renderer::renderIndirectIllumination should only be invoked when in deferred shading mode");
    }
    
    /** Called by DefaultRenderer::renderDeferredShading to configure the inputs to deferred shading. */
    virtual void setDeferredShadingArgs(
        Args&                               args, 
        const shared_ptr<GBuffer>&          gbuffer, 
        const LightingEnvironment&          environment);

    virtual void renderOpaqueSamples
       (RenderDevice*                       rd, 
        Array<shared_ptr<Surface> >&        surfaceArray, 
        const shared_ptr<GBuffer>&          gbuffer, 
        const LightingEnvironment&          environment);

    virtual void renderOpaqueScreenSpaceRefractingSamples
       (RenderDevice*                       rd, 
        Array<shared_ptr<Surface> >&        surfaceArray, 
        const shared_ptr<GBuffer>&          gbuffer, 
        const LightingEnvironment&          environment);

    virtual void renderSortedBlendedSamples       
        (RenderDevice*                      rd, 
        Array<shared_ptr<Surface> >&        surfaceArray, 
        const shared_ptr<GBuffer>&          gbuffer, 
        const LightingEnvironment&          environment);

    virtual void renderOrderIndependentBlendedSamples       
       (RenderDevice*                       rd, 
        Array<shared_ptr<Surface> >&        surfaceArray, 
        const shared_ptr<GBuffer>&          gbuffer, 
        const LightingEnvironment&          environment);

    virtual void allocateAllOITBuffers
       (RenderDevice*                       rd, 
        bool                                highPrecision = false);

    /** Called once for the high res buffer and once for the low res one from allocateAllOITBuffers.
        \param w Desired width of this framebuffer 
        \param h Desired height of this framebuffer 
     */
    virtual void allocateOITFramebufferAttachments
       (RenderDevice*                       rd, 
        const shared_ptr<Framebuffer>&      oitFramebuffer, 
        int                                 w, 
        int                                 h, 
        bool                                highPrecision = false);

    virtual void clearAndRenderToOITFramebuffer
       (RenderDevice*                       rd,
        const shared_ptr<Framebuffer>&      oitFramebuffer,
        Array <shared_ptr<Surface>>&        surfaceArray,
        const shared_ptr<GBuffer>&          gbuffer,
        const LightingEnvironment&          environment);

    /** For OIT */
    virtual void resizeOITBuffersIfNeeded
       (const int                           width,
        const int                           height, 
        const int                           lowResWidth,
        const int                           lowResHeight);

    /**
      For OIT
      \param csNormalTexture May be nullptr
    */
    virtual void computeLowResDepthAndNormals
       (RenderDevice*                       rd, 
        const shared_ptr<Texture>&          csHighResNormalTexture);
    
    DefaultRenderer(const String& className = "DefaultRenderer", const String& namespacePrefix = "G3D::");

public:

    static shared_ptr<Renderer> create() {
        return createShared<DefaultRenderer>();
    }

    /** If true, use deferred shading on all surfaces that can be represented by the GBuffer.
        Default is false.
      */
    void setDeferredShading(bool b) {
        m_deferredShading = b;
    }

    bool deferredShading() const {
        return m_deferredShading;
    }

    /** If true, uses OIT.
        Default is false.
        
        The current implementation is based on:
        
        McGuire and Bavoil, Weighted Blended Order-Independent Transparency, Journal of Computer Graphics Techniques (JCGT), vol. 2, no. 2, 122--141, 2013
        Available online http://jcgt.org/published/0002/02/09/

        This can be turned on in both forward and deferred shading modes.

        This algorithm improves the quality of overlapping transparent surfaces for many scenes, eliminating popping and confusing appearance that can arise
        from imperfect sorting. It is especially helpful in scenes with lots of particles. This technique has relatively low overhead compared to alternative methods.
    */
    void setOrderIndependentTransparency(bool b) {
        m_orderIndependentTransparency = b;
    }

    bool orderIndependentTransparency() const {
        return m_orderIndependentTransparency;
    }

    virtual const String& className() const override{
        static const String n = "DefaultRenderer";
        return n;
    }

    virtual void render
       (RenderDevice*                       rd,
        const shared_ptr<Camera>&           camera,
        const shared_ptr<Framebuffer>&      framebuffer,
        const shared_ptr<Framebuffer>&      depthPeelFramebuffer,
        LightingEnvironment&                lightingEnvironment,
        const shared_ptr<GBuffer>&          gbuffer, 
        const Array<shared_ptr<Surface>>&   allSurfaces) override;
};

} // namespace
