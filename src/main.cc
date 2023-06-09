#include <iostream>
#include <fstream>
#include <string>

//#define USE_ASSIMP

#include "FbxConverter.hh"
#include "BadgerConverter.hh"
#include "AssimpConverter.hh"

int main(int argc, char** argv)
{
    std::string command;

    if (argc > 2) {
        command = argv[1];
    }

    auto doExport = command == "export";
    auto doImport = command == "import";

    if (!(doExport && argc == 5) && !(doImport && argc == 4)) {
        std::cout << "FbxImporter v0.3.0 made by LukeFZ" << std::endl;
        std::cout << "Usage: " << argv[0] << " <command> <arguments>" << std::endl;
        std::cout << "Command 'export': <path to resource packs> <model name> <path to output .fbx>" << std::endl;
        std::cout << "Command 'import': <path to input fbx> <output folder>" << std::endl;
        return -1;
    }

    if (doExport) {
        FbxConverter converter(argv[2]);
        if (!converter.convertToFbx(argv[3], argv[4])) {
            std::cout << "Failed to convert model." << std::endl;
            return -1;
        }
    }

    if (doImport) {
        BadgerConverter converter;
        if (!converter.convertToBadger(argv[2], argv[3])) {
            std::cout << "Failed to convert model." << std::endl;
            return -1;
        }
    }

#ifdef USE_ASSIMP
    AssimpConverter assimpConv;

    if (!assimpConv.convertFbx(argv[3], "output.gltf2", "gltf2")) {
        std::cout << "Failed to convert model to glTF." << std::endl;
        return -1;
    }
#endif

    return 0;
}
