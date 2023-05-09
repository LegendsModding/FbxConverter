#ifdef USE_ASSIMP
#include "AssimpConverter.hh"

#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>

AssimpConverter::AssimpConverter() : 
    importer(),
    exporter() {

}

bool AssimpConverter::convertFbx(const char* fbx, const char* output, const char* format) {
    auto scene = importer.ReadFile(fbx, 0);
    auto result = exporter.Export(scene, format, output);
    if (result != AI_SUCCESS) {
        std::cerr << "Error: conversion from fbx to " << format << "failed. Result: " << result << std::endl;
        return false; 
    }

    return true;
}
#endif