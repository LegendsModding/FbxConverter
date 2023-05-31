#pragma once

#include "BadgerModel.hh"
#include "ResourceLoader.hh"

#include <vector>
#include <fbxsdk.h>

class FbxConverter {
    public:
        explicit FbxConverter(const char* resourcePacksDir);
        ~FbxConverter();

        bool convertToFbx(const char* model, const char* output);
    private:
        bool importMesh(const Badger::Mesh& badgerMesh, size_t meshId);
        bool importBone(const Badger::Bone& badgerBone);
        bool importAnimation(const std::string& name, const Badger::Animation& badgerAnimation);
        bool generateShaderImplementation(const FbxProperty& mayaProp);
        FbxSurfaceMaterial* getMaterial(const std::string& name);
        FbxManager* manager;
        FbxScene* scene;
        ResourceLoader* loader;
        std::unordered_map<std::string, FbxSurfaceMaterial*> createdMaterials;
        FbxImplementation* pbShaderImplementation;
};

static unsigned char DEFAULT_SFX_SHADERGRAPH_COMPRESSED[] = {
#include "shadergraph.brotli.hex"
};
static const int DEFAULT_SFX_SHADERGRAPH_COMPRESSED_LENGTH = sizeof(DEFAULT_SFX_SHADERGRAPH_COMPRESSED);
static const int DEFAULT_SFX_SHADERGRAPH_DECOMPRESSED_LENGTH = 29285;