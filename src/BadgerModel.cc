
#include <iostream>
#include <fstream>
#include <string>

#include "BadgerModel.hh"

using json = nlohmann::json;

namespace Badger {
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


    ModelLoader::ModelLoader(const char* resourcePacks) {
        std::filesystem::path resourcePacksDir(resourcePacks);
        for (auto entry : std::filesystem::directory_iterator(resourcePacks)) {
            if (entry.is_directory()) {
                resourcePaths.push_back(entry);
                auto materialsPath = std::filesystem::path(entry);
                materialsPath /= "materials";
                materialsPath /= "meta_materials";
                materialPaths.push_back(materialsPath);
            }
        }
    }

    bool ModelLoader::loadModel(const char* model) {
        const std::filesystem::path modelPath(model);

        if (!std::filesystem::exists(modelPath)) {
            std::cerr << "Error: model.json does not exist." << std::endl;
            return false;
        }

        std::ifstream modelStream(modelPath, std::ios::in);
        if (!modelStream) {
            std::cerr << "Error: could not open model JSON." << std::endl;
            return false;
        }

        try {
            auto modelJson = json::parse(modelStream);
            this->model = modelJson.get<Model>();
            return true;
        } catch (json::exception e) {
            std::cerr << "Error: failed to parse model JSON. Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool ModelLoader::loadMaterial(const std::string& materialName) {
        if (materials.contains(materialName))
            return true;

        auto filename = materialName + ".json";

        auto i = 0;
        for (auto materialPath : materialPaths) {
            auto resourcePathIndex = i++;
            materialPath /= filename;

            if (!std::filesystem::exists(materialPath)) {
                continue;
            }

            std::ifstream materialStream(materialPath, std::ios::in);
            if (!materialStream) {
                std::cerr << "Error: could not open material JSON. Material: " << materialName << std::endl;
                return false;
            }

            try {
                auto modelJson = json::parse(materialStream);
                auto material = modelJson.get<MetaMaterial>();
                auto resourcePath = resourcePaths.at(resourcePathIndex);
                if (!std::filesystem::exists(std::filesystem::path(resourcePath) / material.info.textures.diffuse)) {
                    auto texFound = false;
                    for (auto it : resourcePaths) {
                        if (std::filesystem::exists(std::filesystem::path(it) / (material.info.textures.diffuse + ".png"))) {
                            texFound = true;
                            resourcePath = it;
                            break;
                        }
                    }

                    if (!texFound) {
                        std::cerr << "Error: could not find texture " << material.info.textures.diffuse << ".png in any resource pack." << std::endl;
                        return false;
                    }
                }

                if (!material.info.textures.diffuse.empty())
                    material.info.textures.diffuse = (std::filesystem::path(resourcePath) / material.info.textures.diffuse).string();
                if (!material.info.textures.coeff.empty())
                    material.info.textures.coeff = (std::filesystem::path(resourcePath) / material.info.textures.coeff).string();          
                if (!material.info.textures.emissive.empty())
                    material.info.textures.emissive = (std::filesystem::path(resourcePath) / material.info.textures.emissive).string();
                if (!material.info.textures.normal.empty())
                    material.info.textures.normal = (std::filesystem::path(resourcePath) / material.info.textures.normal).string();

                materials.insert({materialName, material});
                return true;
            } catch (json::exception e) {
                std::cerr << "Error: failed to parse material JSON. Exception: " << e.what() << std::endl;
                return false;
            }
        }

        std::cerr << "Error: material JSON does not exist. Material: " << materialName << std::endl;
        return false;
    }

    const MetaMaterial& ModelLoader::getMaterial(const std::string& name) {
        return materials.at(name);
    }

    const Model& ModelLoader::getModel() {
        return model;
    }
}