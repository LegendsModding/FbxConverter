#ifdef USE_ASSIMP
#pragma once

#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>

class AssimpConverter {
    public:
        explicit AssimpConverter();
        bool convertFbx(const char* fbx, const char* output, const char* format);
    private:
        Assimp::Importer importer;
        Assimp::Exporter exporter;
};
#endif