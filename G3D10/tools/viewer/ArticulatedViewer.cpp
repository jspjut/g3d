/**
  \file tools/viewer/ArticulatedViewer.cpp

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2019, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/
#include "ArticulatedViewer.h"
#include "App.h"

shared_ptr<ArticulatedViewer::InstructionSurface>   ArticulatedViewer::m_instructions;
shared_ptr<Surface>                                 ArticulatedViewer::m_skyboxSurface;
shared_ptr<GFont>                                   ArticulatedViewer::m_font;

extern App* app;

// Useful for debugging material assignments
static const bool mergeMaterials = true;

ArticulatedViewer::ArticulatedViewer() :
    m_numFaces(0),
    m_numVertices(0),
    m_selectedPart(nullptr),
    m_selectedMesh(nullptr),
    m_selectedTriangleIndex(0),
    m_time(0.0f) {

    if (isNull(m_instructions)) {
        m_instructions  = InstructionSurface::create(Texture::fromFile(System::findDataFile("keyguide.png"), ImageFormat::RGBA8(), Texture::DIM_2D), GFont::fromFile(System::findDataFile("arial.fnt")));
        m_font          = GFont::fromFile(System::findDataFile("arial.fnt"));        
        m_skyboxSurface = SkyboxSurface::create(Texture::fromFile(FilePath::concat(System::findDataFile("whiteroom"), "whiteroom-*.png"), ImageFormat::SRGB8(), Texture::DIM_CUBE_MAP));
    }
}

ArticulatedViewer::~ArticulatedViewer() {
    ArticulatedModel::clearCache();
    Texture::clearCache();
    UniversalMaterial::clearCache();
}

static const float VIEW_SIZE = 30.0f;

void ArticulatedViewer::onInit(const String& filename) {
    ArticulatedModel::clearCache();
    Texture::clearCache();

    m_model = nullptr;
    m_filename = filename;

    m_selectedPart   = nullptr;
    m_selectedMesh   = nullptr;
    m_selectedTriangleIndex = -1;
    m_numFaces       = 0;
    m_numVertices    = 0;
    m_shadowMapDirty = true;

    UniversalMaterial::clearCache();
    
    const RealTime start = System::time();
    if (toLower(FilePath::ext(filename)) == "any") {

        if (toLower(FilePath::ext(FilePath::base(filename))) == "universalmaterial") {

            // Assume that this is an .UniversalMaterial.Any file. Load a square and apply the material
            Any any(Any::TABLE, "ArticulatedModel::Specification");
            any["filename"] = "model/mori_knob/mori_knob.zip/testObj.obj";

            Any preprocess(Any::ARRAY);

            preprocess.append(PARSE_ANY(removeMesh("LTELogo/LTELogo");));
            preprocess.append(PARSE_ANY(removeMesh("Material/Material");));
            preprocess.append(PARSE_ANY(scaleAndOffsetTexCoord0("BackGroundMat/BackGroundMat", 2.5, Vector2(0,0));));
            preprocess.append(PARSE_ANY(scaleAndOffsetTexCoord0("OuterMat/OuterMat", 3, Vector2(0,0));));
            preprocess.append(PARSE_ANY(scaleAndOffsetTexCoord0("InnerMat/InnerMat", 1.6, Vector2(0,0));));
            Any setMaterial(Any::ARRAY, "setMaterial");
            setMaterial.append(Any(Any::ARRAY, "all"));
            setMaterial.append(Any::fromFile(filename));
            preprocess.append(setMaterial);
            preprocess.append(PARSE_ANY(mergeAll(ALL, NONE);));
            preprocess.append(PARSE_ANY(setTwoSided(all(), true);));
            preprocess.append(PARSE_ANY(moveBaseToOrigin();));
            preprocess.append(PARSE_ANY(setCFrame(root(), CFrame::fromXYZYPRDegrees(0, 0, 0, 90, 0, 0));));

            any["preprocess"] = preprocess;

            m_model = ArticulatedModel::create(ArticulatedModel::Specification(any));

            const CFrame& F = CFrame::fromXYZYPRDegrees(-8.2499f, -3.8855f, -1.0488f, -110.44f, -17.228f, 0);
            app->debugCamera()->setFrame(F); 
            app->cameraManipulator()->setFrame(F); 
        } else {

            // Assume that this is an .ArticulatedModel.Any file
            Any any;
            any.load(filename);

            m_model = ArticulatedModel::create(ArticulatedModel::Specification(any));
        }
    } else {
        Any any(Any::TABLE, "ArticulatedModel::Specification");
        any["filename"] = filename;

        // Prevent merging for material debugging
        if (! mergeMaterials) {
            any["meshMergeOpaqueClusterRadius"] = 0;
        }

        const shared_ptr<DefaultRenderer>& renderer = dynamic_pointer_cast<DefaultRenderer>(app->renderer());
        if (notNull(renderer) && mergeMaterials && renderer->orderIndependentTransparency()) {
            any["meshMergeTransmissiveClusterRadius"] = finf();
        }

        // any["stripMaterials"] = true;
        m_model = ArticulatedModel::create(any);
    }
    debugPrintf("%s loaded in %f seconds\n", filename.c_str(), System::time() - start);


    Array<shared_ptr<Surface> > arrayModel;
    if (m_model->usesSkeletalAnimation()) {
        Array<String> animationNames;
        m_model->getAnimationNames(animationNames);
        // TODO: Add support for selecting animations.
        m_model->getAnimation(animationNames[0], m_animation); 
        m_animation.getCurrentPose(0.0f, m_pose);
    } 
    
    m_model->pose(arrayModel, CFrame(), CFrame(), nullptr, &m_pose, &m_pose, Surface::ExpressiveLightScatteringProperties());

    m_model->countTrianglesAndVertices(m_numFaces, m_numVertices);
    
    m_scale = 1;
    m_offset = Vector3::zero();
    bool overwrite = true;
    
    // Find the size of the bounding box of the entire model
    AABox bounds;
    if (arrayModel.size() > 0) {
        
        for (int x = 0; x < arrayModel.size(); ++x) {		
            
            //merges the bounding boxes of all the seperate parts into the bounding box of the entire object
            AABox temp;
            CFrame cframe;
            arrayModel[x]->getCoordinateFrame(cframe);
            arrayModel[x]->getObjectSpaceBoundingBox(temp);
            Box partBounds = cframe.toWorldSpace(temp);
            
            // Some models have screwed up bounding boxes
            if (partBounds.extent().isFinite()) {
                if (overwrite) {
                    partBounds.getBounds(bounds);
                    overwrite = false;
                } else {
                    partBounds.getBounds(temp);
                    bounds.merge(temp);
                }
            }
        }
        
        if (overwrite) {
            // We never found a part with a finite bounding box
            bounds = AABox(Vector3::zero());
        }
        
        Vector3 extent = bounds.extent();
        Vector3 center = bounds.center();
        
        // Scale to X units
        float scale = 1.0f / max(extent.x, max(extent.y, extent.z));
        
        if (scale <= 0) {
            scale = 1;
        }

        if (! isFinite(scale)) {
            scale = 1;
        }

        m_scale = scale;
        scale *= VIEW_SIZE;
        m_offset = -scale * center;
        

        if (! center.isFinite()) {
            center = Vector3();
        }

        // Transform parts in-place
        m_model->scaleWholeModel(scale);

        ArticulatedModel::CleanGeometrySettings csg;
        // Merging vertices is slow and topology hasn't changed at all, so preclude vertex merging
        csg.allowVertexMerging = false; 
        m_model->cleanGeometry(csg);
    }

    // Get the newly transformed animation
    if (m_model->usesSkeletalAnimation()) {
        Array<String> animationNames;
        m_model->getAnimationNames(animationNames);
        // TODO: Add support for selecting animations.
        m_model->getAnimation(animationNames[0], m_animation); 
        m_animation.getCurrentPose(0.0f, m_pose);
    } 

    //saveGeometry();
}


void ArticulatedViewer::saveGeometry() {
    /*
    const MeshAlg::Geometry& geometry = m_model->partArray[0].geometry;
    const Array<Point2>& texCoord     = m_model->partArray[0].texCoordArray;
    
    const ArticulatedModel::Part& part = m_model->partArray[0];

    int numIndices = 0;
    for (int t = 0; t < part.triList.size(); ++t) { 
        numIndices += part.triList[t]->indexArray.size();
    }

    BinaryOutput b("d:/out.bin", G3D_LITTLE_ENDIAN);
    b.writeInt32(numIndices);
    b.writeInt32(geometry.vertexArray.size());
    for (int t = 0; t < part.triList.size(); ++t) {
        const Array<int>& index = part.triList[t]->indexArray;
        for (int i = 0; i < index.size(); ++i) {
            b.writeInt32(index[i]);
        }
    }
    for (int i = 0; i < geometry.vertexArray.size(); ++i) {
        part.cframe.pointToWorldSpace(geometry.vertexArray[i]).serialize(b);
    }
    for (int i = 0; i < geometry.normalArray.size(); ++i) {
        part.cframe.vectorToWorldSpace(geometry.normalArray[i]).serialize(b);
    }
    if (texCoord.size() > 0) {
        for (int i = 0; i < texCoord.size(); ++i) {
            texCoord[i].serialize(b);
        }
    } else {
        for (int i = 0; i < geometry.vertexArray.size(); ++i) {
            Point2::zero().serialize(b);
        }
    }
    b.commit();
    */
}


static void printHierarchy
(const shared_ptr<ArticulatedModel>& model,
 ArticulatedModel::Part*             part,
 const String&                       indent) {
    
    screenPrintf("%s\"%s\")\n", indent.c_str(), part->name.c_str());
    for (int i = 0; i < model->meshArray().size(); ++i) {
        if (model->meshArray()[i]->logicalPart == part) {
            screenPrintf("%s  Mesh \"%s\"\n", indent.c_str(), model->meshArray()[i]->name.c_str());
        }
    }

    for (int i = 0; i < part->childArray().size(); ++i) {
        printHierarchy(model, part->childArray()[i], indent + "  ");
    }
}


void ArticulatedViewer::onPose(Array<shared_ptr<Surface> >& posed3D, Array<shared_ptr<Surface2D> >& posed2D) {
    m_model->pose(posed3D, m_offset, m_offset, nullptr, &m_pose, &m_pose, Surface::ExpressiveLightScatteringProperties());
    posed3D.append(m_skyboxSurface);
    if (app->showInstructions) {
        posed2D.append(m_instructions);
    }
}
    

void ArticulatedViewer::onGraphics3D(RenderDevice* rd, App* app, const shared_ptr<LightingEnvironment>& lighting, Array<shared_ptr<Surface> >& allSurfaces) {
    // app->gbuffer()->setSpecification(m_gbufferSpecification);
    app->gbuffer()->resize(app->framebuffer()->width(), app->framebuffer()->height());

    app->gbuffer()->prepare(rd, app->activeCamera(), 0, -(float)app->previousSimTimeStep(), app->settings().hdrFramebuffer.depthGuardBandThickness, app->settings().hdrFramebuffer.colorGuardBandThickness);

    app->renderer()->render(rd, app->activeCamera(), app->framebuffer(), app->depthPeelFramebuffer(), *lighting, app->gbuffer(), allSurfaces);

    Array<Point3> skeletonLines;    
    m_model->getSkeletonLines(m_pose, m_offset, skeletonLines);
    
    if (skeletonLines.size() > 0) {
        rd->pushState(); {
            rd->setObjectToWorldMatrix(CFrame());
            rd->setDepthTest(RenderDevice::DEPTH_ALWAYS_PASS);
            for (int i = 0; i < skeletonLines.size(); i += 2) {
                Draw::lineSegment(LineSegment::fromTwoPoints(skeletonLines[i], skeletonLines[i + 1]), rd, Color3::red());
            }
        } rd->popState();
    }
    

    //Surface::renderWireframe(rd, posed3D);

    if (notNull(m_selectedMesh)) {
        // Find the index array that matches the selected mesh and render it
        for (int p = 0; p < allSurfaces.size(); ++p) {
            const shared_ptr<UniversalSurface>& s = dynamic_pointer_cast<UniversalSurface>(allSurfaces[p]);

            if (s->gpuGeom()->index == m_selectedMesh->gpuIndexArray) {
                // These have the same index array, so they must be the same surface
                s->renderWireframeHomogeneous(rd, Array<shared_ptr<Surface> >(s), Color3::green(), false);
                break;
            }
        }
    }

    if (! mergeMaterials) {
        screenPrintf("Mesh merging by material DISABLED in this build. Models will render slowly but maintain assignments.\n");
    }


    float x, y, z, yaw, pitch, roll;
    app->activeCamera()->frame().getXYZYPRDegrees(x,y,z,yaw, pitch, roll);
    screenPrintf("[Camera position: Translation(%f, %f, %f) Rotation(%f, %f, %f)]\n", x,y,z,yaw,pitch,roll);
    screenPrintf("[Shown scaled by %f and offset by (%f, %f, %f)]\n",
                 m_scale, m_offset.x, m_offset.y, m_offset.z);
    
    screenPrintf("Model Faces: %d,  Vertices: %d\n", m_numFaces, m_numVertices);
    if (notNull(m_selectedPart)) {
        screenPrintf(" Selected Part `%s', Mesh `%s' (Ctrl-C to copy), Material `%s', cpuIndexArray[%d...%d]\n", 
                     m_selectedPart->name.c_str(), 
                     m_selectedMesh->name.c_str(), 
					 m_selectedMesh->material->name().c_str(),
                     m_selectedTriangleIndex, m_selectedTriangleIndex + 2);
        screenPrintf(" Selected part->cframe = %s\n",
                     m_selectedPart->cframe.toXYZYPRDegreesString().c_str());
    }

    screenPrintf("Hierarchy:");
    // Hierarchy (could do this with a PartCallback)
    for (int i = 0; i < m_model->rootArray().size(); ++i) {
        printHierarchy(m_model, m_model->rootArray()[i], "");
    }
}


void ArticulatedViewer::onSimulation(RealTime rdt, SimTime sdt, SimTime idt) {
   m_time += sdt;
   if (m_model->usesSkeletalAnimation()) {
        m_animation.getCurrentPose(m_time, m_pose);
    }
}


bool ArticulatedViewer::onEvent(const GEvent& e, App* app) {
    if ((e.type == GEventType::MOUSE_BUTTON_DOWN) && (e.button.button == 0) && ! app->userInput->keyDown(GKey::LCTRL)) {
        // Intersect all tri lists with the ray from the camera
        const Ray& ray = app->activeCamera()->worldRay(e.button.x, e.button.y, 
            app->renderDevice->viewport());

        m_selectedPart = nullptr;
        m_selectedMesh = nullptr;
        m_selectedTriangleIndex = -1;
        Model::HitInfo hitInfo;
        float distance = finf();
        const bool hit = m_model->intersect(ray, m_offset, distance, hitInfo, nullptr, nullptr);

        if (hit) {
            m_selectedMesh = m_model->mesh(hitInfo.meshID);
            m_selectedTriangleIndex = hitInfo.primitiveIndex;
            // Output the name of the mesh so that multiple selections can easily
            // be copied from the debug window for processing by other tools
            debugPrintf("\"%s\",\n", m_selectedMesh->name.c_str());
        }

        if (notNull(m_selectedMesh)) {
            m_selectedPart = m_selectedMesh->logicalPart;
        }
        return hit;
    } else if  ((e.type == GEventType::KEY_DOWN) && (e.key.keysym.sym == 'c') && (app->userInput->keyDown(GKey::LCTRL) || app->userInput->keyDown(GKey::RCTRL))) {
        OSWindow::setClipboardText(m_selectedMesh->name);
        return true;
    } else if ((e.type == GEventType::KEY_DOWN) && (e.key.keysym.sym == 'r')) {
        onInit(m_filename);
        return true;
    }

    return false;
}
