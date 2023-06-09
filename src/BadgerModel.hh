#pragma once

#include <string>
#include <vector>
#include <variant>
#include <filesystem>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Badger {
    typedef std::vector<double> Color;
    typedef std::vector<double> Vector3f;
    typedef std::vector<double> Vector2f;

    #pragma region Model Structs

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


    void from_json(const json& j, BoneInfo& p);
    void to_json(json& j, const BoneInfo& p);

    void from_json(const json& j, Bone& p);
    void to_json(json& j, const Bone& p);
    
    void from_json(const json& j, Mesh& p);
    void to_json(json& j, const Mesh& p);
    
    void from_json(const json& j, GeometryDescription& p);
    void to_json(json& j, const GeometryDescription& p);
    
    void from_json(const json& j, Geometry& p);
    void to_json(json& j, const Geometry& p);
    
    void from_json(const json& j, Model& p);
    void to_json(json& j, const Model& p);

    void from_json(const json& j, MetaMaterialTextures& p);
    void to_json(json& j, const MetaMaterialTextures& p);

    void from_json(const json& j, MetaMaterialInfo& p);
    void to_json(json& j, const MetaMaterialInfo& p);

    void from_json(const json& j, MetaMaterial& p);
    void to_json(json& j, const MetaMaterial& p);

    #pragma endregion
    #pragma region Entity Structs

    struct FaceAnimation {
        int colums;
        int rows;
        int blinkFrame;
        int defaultFrame;
    };

    struct EntityComponents {
        std::optional<FaceAnimation> faceAnimation;
        std::vector<std::string> templates;
    };

    struct EntityInfo {
        EntityComponents components;
    };

    struct Entity {
        std::string formatVersion;
        EntityInfo info;
        void applyTemplate(const Entity& templateEntity);
    };

    void from_json(const json& j, Entity& p);
    void to_json(json& j, const Entity& p);

    void from_json(const json& j, EntityInfo& p);
    void to_json(json& j, const EntityInfo& p);

    void from_json(const json& j, EntityComponents& p);
    void to_json(json& j, const EntityComponents& p);

    void from_json(const json& j, FaceAnimation& p);
    void to_json(json& j, const FaceAnimation& p);

    #pragma endregion
    #pragma region Animation Structs

    struct AnimationProperty {
        std::string lerpMode;
        std::vector<double> post;
    };

    struct AnimationBone {
        double lodDistance;
        std::unordered_map<double, AnimationProperty> rotation;
        std::unordered_map<double, AnimationProperty> position;
        std::unordered_map<double, AnimationProperty> scale;
    };

    struct Animation {
        std::string animTimeUpdate;
        std::string blendWeight;
        std::unordered_map<std::string, AnimationBone> bones;
    };

    struct Animations {
        std::string formatVersion;
        std::unordered_map<std::string, Animation> animations;
    };

    void from_json(const json& j, Animations& p);
    void to_json(json& j, const Animations& p);

    void from_json(const json& j, Animation& p);
    void to_json(json& j, const Animation& p);

    void from_json(const json& j, AnimationBone& p);
    void to_json(json& j, const AnimationBone& p);

    void from_json(const json& j, AnimationProperty& p);
    void to_json(json& j, const AnimationProperty& p);

    #pragma endregion
}