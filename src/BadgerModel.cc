
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "BadgerModel.hh"

using json = nlohmann::json;

namespace Badger {

    #pragma region Model Structs

    void from_json(const nlohmann::json& j, BoneInfo& p) {
        j.at("bind_pose_rotation").get_to(p.bindPoseRotation);
    }

    void to_json(json& j, const BoneInfo& p) {
        j = json {
            {"bind_pose_rotation", p.bindPoseRotation}
        };
    }

    void from_json(const nlohmann::json& j, BoneLocator& p) {
        j.at("discard_scale").get_to(p.discardScale);
        j.at("offset").get_to(p.offset);
        j.at("rotation").get_to(p.rotation);
    }

    void to_json(json& j, const BoneLocator& p) {
        j = json {
            {"discard_scale", p.discardScale},
            {"offset", p.offset},
            {"rotation", p.rotation}
        };
    }

    void from_json(const json& j, Bone& p) {
        j.at("\x1\x2\x3\x4\x5__").get_to(p.info);
        if (j.contains("locators")) j.at("locators").get_to(p.locators);
        j.at("name").get_to(p.name);
        j.at("parent").get_to(p.parent);
        j.at("pivot").get_to(p.pivot);
        j.at("scale").get_to(p.scale);
    }

    void to_json(json& j, const Bone& p) {
        j = json {
            {"\x1\x2\x3\x4\x5__", p.info},
            {"name", p.name},
            {"parent", p.parent},
            {"pivot", p.pivot},
            {"scale", p.scale},
        };

        if (!p.locators.empty())
            j["locators"] = p.locators;
    }

    void from_json(const json& j, Mesh& p) {
        if (j.contains("indices")) j.at("indices").get_to(p.indices);
        if (j.contains("color_sets")) j.at("color_sets").get_to(p.colors);
        if (j.contains("model_name")) j.at("model_name").get_to(p.name);
        j.at("meta_material").get_to(p.material);
        j.at("normal_sets").get_to(p.normals);
        j.at("positions").get_to(p.positions);
        j.at("triangles").get_to(p.triangles);
        j.at("uv_sets").get_to(p.uvs);
        j.at("weights").get_to(p.weights);
    }

    void to_json(json& j, const Mesh& p) {
        j = json {
            {"meta_material", p.material},
            {"normal_sets", p.normals},
            {"positions", p.positions},
            {"triangles", p.triangles},
            {"uv_sets", p.uvs},
            {"weights", p.weights}
        };

        if (!p.name.empty())
            j["model_name"] = p.name;

        if (!p.colors.empty())
            j["color_sets"] = p.colors;

        if (!p.indices.empty())
            j["indices"] = p.indices;
    }

    void from_json(const json& j, GeometryDescription& p) {
        j.at("identifier").get_to(p.identifier);
    }

    void to_json(json& j, const GeometryDescription& p) {
        j = json {
            {"identifier", p.identifier},
        };
    }

    void from_json(const json& j, Geometry& p) {
        j.at("bones").get_to(p.bones);
        j.at("meshes").get_to(p.meshes);
        j.at("description").get_to(p.description);
    }

    void to_json(json& j, const Geometry& p) {
        j = json {
            {"bones", p.bones},
            {"meshes", p.meshes},
            {"description", p.description}
        };
    }

    void from_json(const json& j, Model& p) {
        j.at("format_version").get_to(p.formatVersion);
        j.at("minecraft:geometry").get_to(p.geometry);
    }

    void to_json(json& j, const Model& p) {
        j = json {
            {"format_version", p.formatVersion},
            {"minecraft:geometry", p.geometry}
        };
    }

    void from_json(const json& j, MetaMaterialTextures& p) {
        if (j.contains("diffuseMap")) j.at("diffuseMap").get_to(p.diffuse);
        else p.diffuse = std::string();

        if (j.contains("coeffMap")) j.at("coeffMap").get_to(p.coeff);
        else p.coeff = std::string();

        if (j.contains("emissiveMap")) j.at("emissiveMap").get_to(p.emissive);
        else p.emissive = std::string();

        if (j.contains("normalMap")) j.at("normalMap").get_to(p.normal);
        else p.normal = std::string();
    }

    void to_json(json& j, const MetaMaterialTextures& p) {
        j = json {};
        if (!p.diffuse.empty())
            j["diffuseMap"] = p.diffuse;
        if (!p.coeff.empty())
            j["coeffMap"] = p.coeff;
        if (!p.emissive.empty())
            j["emissiveMap"] = p.emissive;
        if (!p.normal.empty())
            j["normalMap"] = p.normal;
    }

    void from_json(const json& j, MetaMaterialInfo& p) {
        if (j.contains("material")) j.at("material").get_to(p.material);
        if (j.contains("culling")) j.at("culling").get_to(p.culling);
        j.at("textures").get_to(p.textures);
    }

    void to_json(json& j, const MetaMaterialInfo& p) {
        j = json {
            {"textures", p.textures}
        };

        if (!p.material.empty())
            j["material"] = p.material;
        if (!p.culling.empty())
            j["culling"] = p.culling;
    }

    void from_json(const json& j, MetaMaterial& p) {
        j.at("format_version").get_to(p.formatVersion);
        for (auto it = ++j.begin(); it != j.end(); ++it) {
            auto matName = it.key();
            auto matNameDelim = matName.rfind(":"); 
            if (matNameDelim != std::string::npos) {
                p.name = matName.substr(0, matNameDelim);
                p.baseName = matName.substr(matNameDelim + 1);
            } else {
                p.name = matName;
            }
            p.info = it.value();
        }
    }

    void to_json(json& j, const MetaMaterial& p) {
        j = json {
            {"format_version", p.formatVersion}
        };

        auto matName = p.baseName.empty() ? p.name : (p.name + ":" + p.baseName);
        j[matName] = p.info;
    }

    #pragma endregion
    #pragma region Entity Structs

    void from_json(const json& j, Entity& p) {
        j.at("format_version").get_to(p.formatVersion);
        j.at("minecraft:client_entity").get_to(p.info);
    }

    void to_json(json& j, const Entity& p) {
        j = json {
            {"format_version", p.formatVersion},
            {"minecraft:client_entity", p.info}
        };
    }

    void from_json(const json& j, EntityInfo& p) {
        j.at("components").get_to(p.components);
    }

    void to_json(json& j, const EntityInfo& p) {
        j = json {
            {"components", p.components}
        };
    }

    void from_json(const json& j, EntityComponents& p) {
        if (j.contains("badger:face_animation")) 
            p.faceAnimation = j.at("badger:face_animation").get<Badger::FaceAnimation>();
        if (j.contains("badger:template")) {
            auto templateObj = j.at("badger:template");
            if (templateObj.is_string()) {
                p.templates.push_back(templateObj.get<std::string>());
            } else {
                templateObj.get_to(p.templates);
            }
        }
    }

    void to_json(json& j, const EntityComponents& p) {
        j = json {};
        if (p.faceAnimation)
            j["badger:face_animation"] = p.faceAnimation.value();

        if (!p.templates.empty()) {
            if (p.templates.capacity() == 1)
                j["badger:template"] = p.templates[0];
            else
                j["badger:template"] = p.templates;
        }
    }

    void from_json(const json& j, FaceAnimation& p) {
        j.at("anim_columns").get_to(p.colums);
        j.at("anim_rows").get_to(p.rows);
        j.at("blink_frame").get_to(p.blinkFrame);
        j.at("default_frame").get_to(p.defaultFrame);
    }

    void to_json(json& j, const FaceAnimation& p) {
        j = json {
            {"anim_columns", p.colums},
            {"anim_rows", p.rows},
            {"blink_frame", p.blinkFrame},
            {"default_frame", p.defaultFrame}
        };
    }

    #pragma endregion

    void Entity::applyTemplate(const Entity& templateEntity) {
        if (!info.components.faceAnimation && templateEntity.info.components.faceAnimation)
            info.components.faceAnimation = templateEntity.info.components.faceAnimation;
    }
}