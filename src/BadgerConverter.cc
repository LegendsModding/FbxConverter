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

    auto fbxFilename = std::filesystem::path(fbx).stem().string();

    std::cout << "Importing fbx." << std::endl;

    bool importStatus = importer->Initialize(fbx, -1, manager->GetIOSettings());

    if (!importStatus) {
        std::cerr << "Error: failed to import fbx." << std::endl;
        return false;
    }

    importer->Import(scene);
    importer->Destroy();

    std::cout << "Imported fbx." << std::endl;

    Badger::Geometry geometry;
    geometry.description.identifier = "geometry." + fbxFilename;

    auto root = scene->GetRootNode();
    auto nodeCount = root->GetChildCount();

    for (auto i = 0; i < nodeCount; i++) {
        auto child = root->GetChild(i);
        auto attribute = child->GetNodeAttribute();
        if (attribute == nullptr) {
            continue;
        }

        auto type = attribute->GetAttributeType();

        if (type == FbxNodeAttribute::eSkeleton) {
            std::cout << "Exporting bones." << std::endl;

            FbxSkeleton* bone = FbxCast<FbxSkeleton>(attribute);
            if (!exportBone(geometry, bone, child)) {
                std::cerr << "Error: failed to export bone." << std::endl;
                return false;
            }
        } else if (type == FbxNodeAttribute::eMesh) {
            std::cout << "Exporting mesh." << std::endl;

            FbxMesh* mesh = FbxCast<FbxMesh>(attribute);
            if (!exportMesh(geometry, mesh, child)) {
                std::cerr << "Error: failed to export mesh." << std::endl;
                return false;
            }
        }
    }

    auto animationCount = scene->GetSrcObjectCount<FbxAnimStack>();
    if (animationCount > 0) {
        std::cout << "Exporting animations." << std::endl;
        for (auto i = 0; i < animationCount; i++) {
            auto stack = scene->GetSrcObject<FbxAnimStack>(i);
            if (!exportAnimation(stack)) {
                std::cerr << "Error: failed to export animation." << std::endl;
                return false;
            }
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
    modelOutput = modelOutput / "models" / "entity";
    std::filesystem::create_directories(modelOutput);
    modelOutput /= (fbxFilename + ".model.json");

    std::ofstream modelOutputStream(modelOutput, std::ios::out);

    json modelJson(model);
    modelOutputStream << modelJson;
    modelOutputStream.close();

    // TODO
    std::cout << "Creating material JSONs." << std::endl;

    auto materialDir = std::filesystem::path(outputDirectory);
    materialDir /= "materials";
    std::filesystem::create_directories(materialDir);

    auto textureDir = std::filesystem::path(outputDirectory) / "textures" / "entity";
    std::filesystem::create_directories(textureDir);

    auto copyAndRedirectTexture = [&](std::string& path) {
        std::filesystem::path originalTexPath(path);
        std::filesystem::path newTexPath(textureDir);
        newTexPath /= originalTexPath.filename();

        if (std::filesystem::exists(newTexPath))
            std::filesystem::remove(newTexPath);

        std::filesystem::copy(originalTexPath, newTexPath);

        path = "textures/entity/" + originalTexPath.stem().string();
    };

    std::ofstream materialOutputStream;
    for (auto& pair : exportedMaterials) {
        if (!pair.second.info.textures.diffuse.empty())
            copyAndRedirectTexture(pair.second.info.textures.diffuse);
        if (!pair.second.info.textures.coeff.empty())
            copyAndRedirectTexture(pair.second.info.textures.coeff);
        if (!pair.second.info.textures.normal.empty())
            copyAndRedirectTexture(pair.second.info.textures.normal);
        if (!pair.second.info.textures.emissive.empty())
            copyAndRedirectTexture(pair.second.info.textures.emissive);

        auto materialOutputPath = std::filesystem::path(materialDir);
        materialOutputPath /= (pair.second.name + ".json");
        json materialJson(pair.second);
        materialOutputStream.open(materialOutputPath);
        materialOutputStream << materialJson;
        materialOutputStream.close();
    }

    if (!exportedAnimations.empty()) {
        std::cout << "Creating animation JSON." << std::endl;

        Badger::Animations animations {
            .formatVersion = "1.8.0",
            .animations = exportedAnimations
        };

        auto animationsPath = std::filesystem::path(outputDirectory) / "animations";
        std::filesystem::create_directories(animationsPath);

        animationsPath /= (fbxFilename + ".animations.json");

        std::ofstream animationsOutputStream(animationsPath, std::ios::out);
        json animationsJson(animations);
        animationsOutputStream << animationsJson;
        animationsOutputStream.close();
    }

    std::cout << "Export finished." << std::endl;

    return true;
}

bool BadgerConverter::exportMesh(Badger::Geometry& geometry, const FbxMesh* mesh, const FbxNode* node) {
    Badger::Mesh badgerMesh;
    auto controlPointCount = mesh->GetControlPointsCount();
    badgerMesh.positions.reserve(controlPointCount);
    badgerMesh.weights.reserve(controlPointCount);

    std::cout << "Exporting positions." << std::endl;

    for (auto i = 0; i < controlPointCount; i++) {
        double* controlPoint = mesh->GetControlPoints()[i];
        std::vector<double> position{&controlPoint[0], &controlPoint[3]};
        badgerMesh.positions.push_back(position);
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


    auto deformer = mesh->GetDeformer(0);
    if (deformer != nullptr && deformer->GetDeformerType() == FbxDeformer::eSkin) {
        std::cout << "Exporting skin information." << std::endl;
        FbxSkin* skin = FbxCast<FbxSkin>(deformer);

        badgerMesh.weights.resize(controlPointCount);
        badgerMesh.indices.resize(controlPointCount);

        for (auto i = 0; i < skin->GetClusterCount(); i++) {
            FbxCluster* cluster = skin->GetCluster(i);
            auto link = cluster->GetLink();
            if (link == nullptr) {
                std::cerr << "Error: skin cluster had no bone linked to it." << std::endl;
                return false;
            }

            auto weightsPointer = cluster->GetControlPointWeights();

            if (cluster->GetControlPointIndicesCount() != controlPointCount
                || weightsPointer == nullptr) {
                std::cerr << "Error: skin cluster weight count did not match control point count." << std::endl;
                return false;
            }

            auto boneName = link->GetName();

            for (auto j = 0; j < controlPointCount; j++) {
                badgerMesh.indices.at(j).push_back(boneName);
                badgerMesh.weights.at(j).push_back(weightsPointer[j]);
            }
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

    exportMaterial(material);

    badgerMesh.material = exportedMaterials.at(material->GetName()).name;

    geometry.meshes.push_back(badgerMesh);

    return true;
}

bool BadgerConverter::exportMaterial(const FbxSurfaceMaterial* material) {
    const std::string name(material->GetName());
    if (auto it = exportedMaterials.find(name); it != exportedMaterials.end())
        return true;

    Badger::MetaMaterial metaMaterial;
    auto matDelim = name.rfind("___");
    if (matDelim != std::string::npos) {
        metaMaterial.name = name.substr(0, matDelim);
        metaMaterial.baseName = name.substr(matDelim + 3);
    } else {
        metaMaterial.name = name;
    }

    metaMaterial.formatVersion = "1.8.0";
    metaMaterial.info.culling = "none";

    auto assignTextureName = [&](const char* isUsedProperty, const char* texProperty, std::string& badgerProp) -> bool {
        auto usedProp = material->FindPropertyHierarchical(isUsedProperty);
        if (!usedProp.IsValid()) {
            std::cerr << "Could not find property " << isUsedProperty << " on material." << std::endl;
            return false;
        }

        if (usedProp.Get<bool>()) {
            auto texProp = material->FindPropertyHierarchical(texProperty);
            if (!texProp.IsValid()) {
                std::cerr << "Could not find property " << texProperty << " on material." << std::endl;
                return false;
            }

            auto connectedTexture = texProp.GetSrcObject<FbxFileTexture>(0);
            if (connectedTexture == nullptr) {
                std::cerr << "Material has no connected texture for property " << texProperty << std::endl;
                return false;
            }

            badgerProp = connectedTexture->GetFileName();
        }

        return true;
    };

    assignTextureName("Maya|use_color_map", "Maya|TEX_color_map", metaMaterial.info.textures.diffuse);
    assignTextureName("Maya|use_normal_map", "Maya|TEX_normal_map", metaMaterial.info.textures.normal);
    assignTextureName("Maya|use_metallic_map", "Maya|TEX_metallic_map", metaMaterial.info.textures.coeff);
    assignTextureName("Maya|use_emissive_map", "Maya|TEX_emissive_map", metaMaterial.info.textures.emissive);

    exportedMaterials.insert({name, metaMaterial});
    return true;
}

bool BadgerConverter::exportBone(Badger::Geometry& geometry, const FbxSkeleton* skeleton, const FbxNode* node) {    
    auto convertVector = [&](FbxDouble3 vec) -> std::vector<double> {
        return std::vector<double>(&vec[0], &vec[3]);
    };
    
    Badger::Bone badgerBone {};
    badgerBone.name = node->GetName();

    badgerBone.scale = convertVector(node->LclScaling.Get());
    badgerBone.pivot = convertVector(node->LclTranslation.Get());
    badgerBone.info.bindPoseRotation = convertVector(node->LclRotation.Get());

    if (auto parent = node->GetParent(); parent != nullptr) {
        auto parentAttribute = parent->GetNodeAttribute();
        if (parentAttribute != nullptr && parentAttribute->GetAttributeType() == FbxNodeAttribute::eSkeleton) {
            badgerBone.parent = parent->GetName();
        }
    }

    for (auto i = 0; i < node->GetChildCount(); i++) {
        auto child = node->GetChild(i);
        auto attribute = child->GetNodeAttribute();
        if (attribute == nullptr || attribute->GetAttributeType() != FbxNodeAttribute::eNull)
            continue;

        FbxTransform::EInheritType inheritType;
        child->GetTransformationInheritType(inheritType);

        Badger::BoneLocator locator {};
        locator.discardScale = inheritType == FbxTransform::eInheritRrs;
        locator.offset = convertVector(child->LclTranslation.Get());
        locator.rotation = convertVector(child->LclRotation.Get());
        badgerBone.locators.insert({child->GetName(), locator});
    }

    geometry.bones.push_back(badgerBone);

    // Now we iterate over the children a second time. Costs performance, but the exported bones are not reversed in their order anymore.

    for (auto i = 0; i < node->GetChildCount(); i++) {
        auto child = node->GetChild(i);
        auto attribute = child->GetNodeAttribute();
        if (attribute == nullptr || attribute->GetAttributeType() != FbxNodeAttribute::eSkeleton)
            continue;

        exportBone(geometry, FbxCast<FbxSkeleton>(attribute), child);
    }

    return true;
}

bool BadgerConverter::exportAnimation(FbxAnimStack* stack) {
    std::string name(stack->GetName());

    if (!name.starts_with("animation."))
        name = "animation." + name;

    Badger::Animation badgerAnimation {
        .animTimeUpdate = "(query.anim_time + (query.delta_time * 1))",
        .blendWeight = "1",
        .bones = {}
    };

    scene->SetCurrentAnimationStack(stack);

    /*FbxTime animPeriodTime;
    animPeriodTime.SetSecondDouble(0.0333333333333);

    auto result = stack->BakeLayers(scene->GetAnimationEvaluator(), stack->LocalStart.Get(), stack->LocalStop.Get(), animPeriodTime);
    if (!result) {
        std::cerr << "Error while baking animation layers." << std::endl;
        return false;
    }*/

    auto baseLayer = stack->GetMember<FbxAnimLayer>(0);
    if (baseLayer == nullptr) {
        std::cerr << "Error: animation has no base layer!" << std::endl;
        return false;
    }
    
    for (auto i = 0; i < baseLayer->GetSrcObjectCount<FbxAnimCurveNode>(); i++) {
        auto curveNode = baseLayer->GetSrcObject<FbxAnimCurveNode>(i);
        if (curveNode->GetChannelsCount() != 3 && curveNode->GetDstPropertyCount() != 1) {
            std::cerr << "Error: unsupported curve node in animation." << std::endl;
            return false;
        }

        auto connectedProperty = curveNode->GetDstProperty(0);
        std::string propertyName(connectedProperty.GetName());
        if (propertyName != "Lcl Translation" && propertyName != "Lcl Rotation") {
            std::cerr << "Error: unsupported connected curve property in animation. Only translation and rotation is supported." << std::endl;
            return false;
        }

        auto boneName = connectedProperty.GetParent().GetName();
        
        auto curveX = connectedProperty.GetCurve(baseLayer, FBXSDK_CURVENODE_COMPONENT_X, false);
        auto curveY = connectedProperty.GetCurve(baseLayer, FBXSDK_CURVENODE_COMPONENT_Y, false);
        auto curveZ = connectedProperty.GetCurve(baseLayer, FBXSDK_CURVENODE_COMPONENT_Z, false);

        auto defaultValues = connectedProperty.Get<FbxDouble3>();
        //std::cout << "def " << defaultValues[0] << " y " << defaultValues[1] << " z " << defaultValues[2] << std::endl;

        if (curveX == nullptr || curveY == nullptr || curveZ == nullptr) {
            std::cerr << "Error: one or more required curve components are missing." << std::endl;
            return false;
        }

        std::unordered_map<double, Badger::AnimationProperty> keyframeData;

        auto keyCount = curveX->KeyGetCount();
        for (auto j = 0; j < keyCount; j++) {
            auto xKey = curveX->KeyGet(j);
            auto yKey = curveY->KeyGet(j);
            auto zKey = curveZ->KeyGet(j);

            //std::cout << xKey.GetTime().GetSecondDouble() << std::endl;
            //std::cout << "key " << xKey.GetValue() << " y " << yKey.GetValue() << " z " << zKey.GetValue() << std::endl;

            keyframeData.insert({
                xKey.GetTime().GetSecondDouble(),
                {
                    .lerpMode = "catmullrom",
                    .post = {
                        xKey.GetValue() - defaultValues[0],
                        yKey.GetValue() - defaultValues[1], 
                        zKey.GetValue() - defaultValues[2]
                    }
                }
            });
        }

        auto namePtr = boneName.Buffer();

        if (!badgerAnimation.bones.contains(namePtr)) {
            badgerAnimation.bones.insert({namePtr, {
                .lodDistance = 0
            }});
        }

        if (propertyName == "Lcl Translation")
            badgerAnimation.bones[namePtr].position = keyframeData;
        else
            badgerAnimation.bones[namePtr].rotation = keyframeData;
    }

    exportedAnimations.insert({name, badgerAnimation});
    return true;
}