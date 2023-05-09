#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Badger {
    typedef std::vector<double> Color;
    typedef std::vector<double> Vector3f;
    typedef std::vector<double> Vector2f;

    struct BoneInfo {
        Vector3f bindPoseRotation;
    };

    struct BoneLocator {
        bool discardScale;
        Vector3f offset;
        Vector3f rotation;
    };

    struct Bone {
        BoneInfo info;
        std::unordered_map<std::string, BoneLocator> locators;
        std::string name;
        std::string parent;
        Vector3f pivot;
        Vector3f scale;
    };

    struct Mesh {
        // General info
        std::string name;
        std::string material;

        // Vertex info
        std::vector<Vector3f> positions;
        std::vector<int> triangles;

        // Bone info
        std::vector<std::vector<std::string>> indices;
        std::vector<std::vector<double>> weights;

        // Normal info
        std::vector<std::vector<Color>> colors;
        std::vector<std::vector<Vector3f>> normals;
        std::vector<std::vector<Vector2f>> uvs;
    };

    struct GeometryDescription {
        std::string identifier;
    };

    struct Geometry {
        std::vector<Bone> bones;
        std::vector<Mesh> meshes;
        GeometryDescription description;
    };

    struct Model {
        std::string formatVersion;
        std::vector<Geometry> geometry;
    };

    struct MetaMaterialTextures {
        std::string diffuse;
        std::string coeff;
        std::string emissive;
        std::string normal;
    };

    struct MetaMaterialInfo {
        std::string material;
        std::string culling;
        MetaMaterialTextures textures;
    };

    struct MetaMaterial {
        std::string formatVersion;
        std::string name;
        std::string baseName;
        MetaMaterialInfo info;
    };

    void to_json(json& j, const BoneInfo& p);
    void to_json(json& j, const Bone& p);
    void to_json(json& j, const Mesh& p);
    void to_json(json& j, const GeometryDescription& p);
    void to_json(json& j, const Geometry& p);
    void to_json(json& j, const Model& p);
    void to_json(json& j, const MetaMaterialTextures& p);
    void to_json(json& j, const MetaMaterialInfo& p);
    void to_json(json& j, const MetaMaterial& p);

    class ModelLoader {
        public:
            explicit ModelLoader(const char* resourcePacksFolder);
            bool loadModel(const char* modelPath);
            bool loadMaterial(const std::string& name);
            const MetaMaterial& getMaterial(const std::string& name);
            const Model& getModel();
        private:
            Model model;
            std::unordered_map<std::string, MetaMaterial> materials;
            std::vector<std::filesystem::path> resourcePaths;
            std::vector<std::filesystem::path> materialPaths;
    };
}