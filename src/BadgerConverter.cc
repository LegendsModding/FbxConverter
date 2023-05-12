#include "BadgerConverter.hh"

#include <iostream>
#include <fstream>

BadgerConverter::BadgerConverter() : model() {
    manager = FbxManager::Create();
    auto ioSettings = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ioSettings);
    scene = FbxScene::Create(manager, "ExportedScene");

    model.formatVersion = "1.14.0";
}

bool BadgerConverter::convertToBadger(const char* fbx, const char* outputDirectory) {
    auto importer = FbxImporter::Create(manager, "");

    auto fbxFilename = std::filesystem::path(fbx).filename().string();

    std::cout << "Importing fbx." << std::endl;

    bool importStatus = importer->Initialize(fbx, -1, manager->GetIOSettings());

    if (!importStatus) {
        std::cerr << "Error: failed to import fbx." << std::endl;
        return false;
    }

    importer->Import(scene);
    importer->Destroy();

    Badger::Geometry geometry;
    geometry.description.identifier = "geometry." + fbxFilename;

    auto root = scene->GetRootNode();
    auto meshCount = root->GetChildCount();

    std::cout << "Exporting " << meshCount << " meshes." << std::endl;

    for (auto i = 0; i < meshCount; i++) {
        auto child = root->GetChild(i);
        auto attribute = child->GetNodeAttribute();
        if (attribute == nullptr) {
            continue;
        }

        if (attribute->GetAttributeType() != FbxNodeAttribute::eMesh) {
            continue;
        }

        auto mesh = dynamic_cast<FbxMesh*>(attribute);
        if (!exportMesh(geometry, mesh, child)) {
            std::cerr << "Error: failed to export mesh." << std::endl;
            return false;
        }
    }


    if (geometry.bones.empty()) {
        Badger::Bone bone {
            .info {
                .bindPoseRotation {
                    0, 0, 0
                }
            },
            .name = "minecraft:geometry_root_bone",
            .parent = "",
            .pivot = {
                0, 0, 0
            },
            .scale {
                1, 1, 1
            }
        };

        geometry.bones.push_back(bone);
    }

    model.geometry.push_back(geometry);

    std::cout << "Creating model JSON." << std::endl;

    std::filesystem::path outputPath(outputDirectory);
    std::filesystem::create_directories(outputPath);

    std::filesystem::path modelOutput(outputPath);
    modelOutput /= (fbxFilename + ".model.json");

    std::ofstream modelOutputStream(modelOutput, std::ios::out);

    nlohmann::json modelJson(model);
    modelOutputStream << modelJson;
    modelOutputStream.close();

    std::cout << "Creating material JSONs." << std::endl;

    auto materialDir = std::filesystem::path(outputDirectory);
    materialDir /= "materials";
    std::filesystem::create_directories(materialDir);

    std::ofstream materialOutputStream;
    for (const auto& pair : exportedMaterials) {
        auto materialOutputPath = std::filesystem::path(materialDir);
        materialOutputPath /= (pair.first + ".json");
        json materialJson(pair.second);
        materialOutputStream.open(materialOutputPath);
        materialOutputStream << materialJson;
        materialOutputStream.close();
    }

    return true;
}

bool BadgerConverter::exportMesh(Badger::Geometry& geometry, const FbxMesh* mesh, const FbxNode* node) {
    Badger::Mesh badgerMesh;
    auto controlPointCount = mesh->GetControlPointsCount();
    badgerMesh.positions.reserve(controlPointCount);
    badgerMesh.weights.reserve(controlPointCount);

    std::cout << "Exporting positions and weights." << std::endl;

    for (auto i = 0; i < controlPointCount; i++) {
        double* controlPoint = mesh->GetControlPoints()[i];
        std::vector<double> position{&controlPoint[0], &controlPoint[3]};
        badgerMesh.positions.push_back(position);
        badgerMesh.weights.push_back({controlPoint[3]});

    }

    std::cout << "Exporting triangles." << std::endl;

    auto polygonCount = mesh->GetPolygonCount();
    auto verticesCount = mesh->GetPolygonVertexCount();
    if (polygonCount * 3 != mesh->GetPolygonVertexCount()) {
        std::cerr << "Error: the mesh is not triangulated." << std::endl;
        return false;
    }

    badgerMesh.triangles.reserve(polygonCount);
    auto vertices = mesh->GetPolygonVertices();
    badgerMesh.triangles.insert(badgerMesh.triangles.begin(), &vertices[0], &vertices[verticesCount]);

    std::cout << "Exporting normals." << std::endl;

    auto normalsCount = mesh->GetElementNormalCount();
    badgerMesh.normals.reserve(normalsCount);
    if (normalsCount != 0) {
        for (auto i = 0; i < normalsCount; i++) {
            auto normalElement = mesh->GetElementNormal(i);
            auto normalCount = normalElement->GetDirectArray().GetCount();

            std::vector<std::vector<double>> normalsVec;
            normalsVec.reserve(normalCount);

            for (auto j = 0; j < normalCount; j++) {
                auto normal = normalElement->GetDirectArray()[j];
                std::vector<double> normalVec(&normal[0], &normal[4]);
                normalsVec.push_back(normalVec);
            }

            badgerMesh.normals.push_back(normalsVec);
        }
    }

    std::cout << "Exporting UVs." << std::endl;

    auto uvsCount = mesh->GetElementUVCount();
    badgerMesh.uvs.reserve(uvsCount);
    if (uvsCount != 0) {
        for (auto i = 0; i < uvsCount; i++) {
            auto uvElement = mesh->GetElementUV(i);
            auto uvCount = uvElement->GetDirectArray().GetCount();

            std::vector<std::vector<double>> uvsVec;
            uvsVec.reserve(uvCount);

            for (auto j = 0; j < uvCount; j++) {
                auto uv = uvElement->GetDirectArray()[j];
                std::vector<double> uvVec {
                    uv[0], 1 - uv[1]
                };
                uvsVec.push_back(uvVec);
            }

            badgerMesh.uvs.push_back(uvsVec);
        }
    }

    std::cout << "Exporting vertex colors." << std::endl;

    auto colorsCount = mesh->GetElementVertexColorCount();
    badgerMesh.colors.reserve(colorsCount);
    if (colorsCount != 0) {
        for (auto i = 0; i < colorsCount; i++) {
            auto colorElement = mesh->GetElementVertexColor(i);
            auto colorCount = colorElement->GetDirectArray().GetCount();

            std::vector<std::vector<double>> colorsVec;
            colorsVec.reserve(colorCount);

            for (auto j = 0; j < colorCount; j++) {
                auto color = colorElement->GetDirectArray()[j];
                std::vector<double> colorVec(&color[0], &color[4]);
                colorsVec.push_back(colorVec);
            }

            badgerMesh.colors.push_back(colorsVec);
        }
    }

    std::cout << "Exporting material." << std::endl;

    auto materialCount = mesh->GetElementMaterialCount();
    if (materialCount == 0) {
        std::cerr << "Error: mesh has no material." << std::endl;
        return false;
    } else if (materialCount > 1) {
        std::cerr << "Warning: mesh has more than one material, using first." << std::endl;
    }

    auto materialIndex = mesh->GetElementMaterial()->GetIndexArray().GetFirst();
    auto material = node->GetMaterial(materialIndex);
    badgerMesh.material = material->GetName();

    exportMaterial(material);

    geometry.meshes.push_back(badgerMesh);

    return true;
}

bool BadgerConverter::exportMaterial(const FbxSurfaceMaterial* material) {
    const std::string name(material->GetName());
    if (auto it = exportedMaterials.find(name); it != exportedMaterials.end())
        return true;

    Badger::MetaMaterial metaMaterial;
    Badger::MetaMaterialInfo materialInfo;
    auto matDelim = name.rfind("___");
    if (matDelim != std::string::npos) {
        metaMaterial.name = name.substr(0, matDelim);
        metaMaterial.baseName = name.substr(matDelim + 3);
    } else {
        metaMaterial.name = name;
    }

    metaMaterial.formatVersion = "1.8.0";
    materialInfo.culling = "none";

    exportedMaterials.insert({name, metaMaterial});
    return true;
}