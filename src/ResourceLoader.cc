#include "ResourceLoader.hh"
#include "BadgerModel.hh"

#include <fstream>
#include <filesystem>
#include <vector>
#include <iostream>
#include <optional>

ResourceLoader::ResourceLoader(const char* resourcePacksDirectory) {
    for (const auto& entry : std::filesystem::directory_iterator(resourcePacksDirectory)) {
        if (entry.is_directory()) {
            resourcePacks.push_back(entry);
        }
    };
}

std::optional<const Badger::Model> ResourceLoader::getModel(const std::string& name) {
    if (const auto& it = modelCache.find(name); it != modelCache.end())
        return it->second;

    return loadModel(name);
}

std::optional<const Badger::MetaMaterial> ResourceLoader::getMaterial(const std::string& name) {
    if (const auto& it = materialCache.find(name); it != materialCache.end())
        return it->second;

    return loadMaterial(name);
}

std::optional<const Badger::Entity> ResourceLoader::getEntity(const std::string& name) {
    if (const auto& it = entityCache.find(name); it != entityCache.end())
        return it->second;

    return loadEntity(name);
}

std::optional<const Badger::Model> ResourceLoader::loadModel(const std::string& name) {
    auto filename = name + ".model.json";
    for (const auto& packDirectory : resourcePacks) {
        auto modelPath = packDirectory / "models" / "entity" / filename;
        if (!std::filesystem::exists(modelPath)) {
            continue;
        }

        std::ifstream file(modelPath);
        if (!file) {
            std::cerr << "Error: could not open model at path " << modelPath << "." << std::endl;
            return {};
        }

        try {
            auto modelJson = json::parse(file);
            std::vector<Badger::Geometry> geometry;

            for (auto geoJson : modelJson.at("minecraft:geometry")) {
                Badger::Geometry geo;
                geoJson.at("description").get_to(geo.description);
                geoJson.at("meshes").get_to(geo.meshes);
                if (geoJson.at("bones").is_string()) {
                    auto inheritedModel = getModel(geoJson.at("bones").get<std::string>().substr(9));
                    if (!inheritedModel)
                        return {};

                    geo.bones = inheritedModel->geometry[0].bones;
                } else {
                    geoJson.at("bones").get_to(geo.bones);
                }
                geometry.push_back(geo);
            }

            Badger::Model model = {
                .formatVersion = "1.8.0",
                .geometry = geometry
            };
            modelCache.insert({name, model});
            return model;
        } catch (json::exception& e) {
            std::cerr << "Error: failed to parse json (" << e.what() << ")" << std::endl;
            return {};
        }
    }

    std::cerr << "Error: could not find model " << name << " in any resource pack." << std::endl;
    return {};
}

std::optional<const Badger::MetaMaterial> ResourceLoader::loadMaterial(const std::string& name) {
    auto filename = name + ".json";
    for (const auto& packDirectory : resourcePacks) {
        auto materialPath = packDirectory / "materials" / "meta_materials" / filename;
        if (!std::filesystem::exists(materialPath)) {
            continue;
        }

        std::ifstream file(materialPath);
        if (!file) {
            std::cerr << "Error: could not open material at path " << materialPath << "." << std::endl;
            return {};
        }

        try {
            auto material = json::parse(file).get<Badger::MetaMaterial>();
            
            auto texturePackDirectory = packDirectory;
            if (!std::filesystem::exists(texturePackDirectory / material.info.textures.diffuse)) {
                auto texturesFound = false;
                for (const auto& textureDirectory : resourcePacks) {
                    if (std::filesystem::exists(textureDirectory / (material.info.textures.diffuse + ".png"))) {
                        texturesFound = true;
                        texturePackDirectory = textureDirectory;
                        break;
                    }
                }

                if (!texturesFound) {
                    std::cerr << "Error: failed to find texture " << material.info.textures.diffuse << " in any resource pack." << std::endl;
                    return {};
                }
            }

            if (!material.info.textures.diffuse.empty())
                material.info.textures.diffuse = (texturePackDirectory / material.info.textures.diffuse).string();
            if (!material.info.textures.coeff.empty())
                material.info.textures.coeff = (texturePackDirectory / material.info.textures.coeff).string();          
            if (!material.info.textures.emissive.empty())
                material.info.textures.emissive = (texturePackDirectory / material.info.textures.emissive).string();
            if (!material.info.textures.normal.empty())
                material.info.textures.normal = (texturePackDirectory / material.info.textures.normal).string();

            // TODO: Add base material importing
            materialCache.insert({name, material});
            return material;
        } catch (json::exception& e) {
            std::cerr << "Error: failed to parse json (" << e.what() << ")" << std::endl;
            return {};
        }
    }

    std::cerr << "Error: could not find material " << name << " in any resource pack." << std::endl;
    return {};
}

std::optional<const Badger::Entity> ResourceLoader::loadEntity(const std::string& name) {
    auto filename = name + ".entity.json";
    for (const auto& packDirectory : resourcePacks) {
        auto path = packDirectory / "entity" / filename;
        if (!std::filesystem::exists(path)) {
            continue;
        }

        std::ifstream file(path);
        if (!file) {
            std::cerr << "Error: could not open entity at path " << path << "." << std::endl;
            return {};
        }

        try {
            auto entity = json::parse(file).get<Badger::Entity>();
            if (!entity.info.components.templates.empty()) {
                for (const auto& parent : entity.info.components.templates) {
                    auto parentEntity = getEntity(parent.substr(parent.find(':') + 1));
                    if (!parentEntity) {
                        return {};
                    }
                    entity.applyTemplate(parentEntity.value());
                }
            }

            entityCache.insert({name, entity});
            return entity;
        } catch (json::exception& e) {
            std::cerr << "Error: failed to parse json (" << e.what() << ")" << std::endl;
            return {};
        }
    }

    std::cerr << "Error: could not find entity " << name << " in any resource pack." << std::endl;
    return {};
}