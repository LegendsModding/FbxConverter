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
        bool exportBone(Badger::Geometry& geometry, const FbxSkeleton* skeleton, const FbxNode* node);
        bool exportMaterial(const FbxSurfaceMaterial* material);
        bool exportAnimation(FbxAnimStack* stack);
        FbxManager* manager;
        FbxScene* scene;
        std::unordered_map<std::string, Badger::MetaMaterial> exportedMaterials;
        std::unordered_map<std::string, Badger::Animation> exportedAnimations;
        Badger::Model model;
};