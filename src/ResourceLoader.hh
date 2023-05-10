#include "BadgerModel.hh"

#include <vector>
#include <filesystem>
#include <optional>

class ResourceLoader {
    public:
        explicit ResourceLoader(const char* resourcePacksDirectory);
        std::optional<const Badger::Model> getModel(const std::string& name);
        std::optional<const Badger::MetaMaterial> getMaterial(const std::string& name);
        std::optional<const Badger::Entity> getEntity(const std::string& name);
        // Badger::Animation getAnimation(const std::string& name);
    private:
        std::optional<const Badger::Model> loadModel(const std::string& name);
        std::optional<const Badger::MetaMaterial> loadMaterial(const std::string& name);
        std::optional<const Badger::Entity> loadEntity(const std::string& name);

        std::vector<std::filesystem::path> resourcePacks;
        std::unordered_map<std::string, Badger::MetaMaterial> materialCache;
        std::unordered_map<std::string, Badger::Model> modelCache;
        std::unordered_map<std::string, Badger::Entity> entityCache;
};