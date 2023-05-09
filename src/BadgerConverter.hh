#pragma once

#include "BadgerModel.hh"

#include <vector>
#include <fbxsdk.h>

class BadgerConverter {
    public:
        explicit BadgerConverter();
        //~BadgerConverter();

        bool convertToBadger(const char* fbx, const char* outputFolder);
    private:
        bool exportMesh(Badger::Geometry& geometry, const FbxMesh* mesh, const FbxNode* node);
        bool exportMaterial(const FbxSurfaceMaterial* material);
        FbxManager* manager;
        FbxScene* scene;
        std::unordered_map<std::string, Badger::MetaMaterial> exportedMaterials;
        Badger::Model model;
};