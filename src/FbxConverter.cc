#include "FbxConverter.hh"
#include "BadgerModel.hh"

#include <iostream>

#include <brotli/decode.h>

#define BINARY

FbxConverter::FbxConverter(const char* resourcePacksDir) {
    manager = FbxManager::Create();
    auto ioSettings = FbxIOSettings::Create(manager, IOSROOT);
#ifdef BINARY
    ioSettings->SetBoolProp(EXP_FBX_EMBEDDED, true);
#endif
    manager->SetIOSettings(ioSettings);
    loader = new ResourceLoader(resourcePacksDir);
    pbShaderImplementation = nullptr;
    scene = nullptr;
}

FbxConverter::~FbxConverter() {
    if (pbShaderImplementation != nullptr)
        pbShaderImplementation->Destroy(true);
    
    if (scene != nullptr)
        scene->Destroy(true);
    
    if (manager != nullptr)
        manager->Destroy();

    delete loader;
}

bool FbxConverter::convertToFbx(const char* model, const char* output) {
    std::cout << "Parsing model JSON." << std::endl;

    std::string modelName(model);
    auto modelData = loader->getModel(modelName);
    if (!modelData) {
        return false;
    }

    if (modelData->geometry.capacity() != 1) {
        std::cerr << "Error: model has more than one geometry - this is not supported." << std::endl;
        return false;
    }
    auto geometry = modelData->geometry.at(0);

    std::cout << "Importing model." << std::endl;

    scene = FbxScene::Create(manager, "ImportedModel");

    std::cout << "Importing bones." << std::endl;

    for (const auto& bone : geometry.bones) {
        std::cout << "Importing bone " << bone.name << std::endl;
        if (!importBone(bone)) {
            std::cerr << "Error: failed to import bone." << std::endl;
            return false;
        }
    }

    std::cout << "Importing meshes." << std::endl;

    auto meshId = 0;
    for (const auto& mesh : geometry.meshes) {
        std::cout << "Importing mesh #" << meshId << std::endl;
        if (!loader->getMaterial(mesh.material)) {
            return false;
        }

        if (!importMesh(mesh, meshId)) {
            std::cerr << "Error: failed to import mesh #" << meshId << std::endl;
            return false;
        }
        std::cout << "Finished importing mesh #" << meshId << std::endl;
        meshId++;
    }

    auto entity = loader->getEntity(modelName);
    if (entity && entity->info.components.faceAnimation) {
        std::cout << "Setting UV offset and scale for face material." << std::endl;
        auto matName = "mat_" + modelName + "_face";
        if (auto it = createdMaterials.find(matName); it == createdMaterials.end()) {
            std::cerr << "Failed to find face material " << matName << "." << std::endl;
            return false;
        } else {
            auto faceMaterial = it->second;
            auto faceAnim = entity->info.components.faceAnimation.value();
            auto widthOffset = 1.0 / faceAnim.colums;
            auto heightOffset = 1.0 / faceAnim.rows;
            faceMaterial->FindPropertyHierarchical("Maya|uv_scale").Set(FbxVector2(widthOffset, heightOffset));
            faceMaterial->FindPropertyHierarchical("Maya|uv_offset").Set(FbxVector2(widthOffset * faceAnim.defaultFrame, heightOffset * faceAnim.defaultFrame));
        }
    }

    auto animations = loader->getAnimations(modelName);
    if (animations.has_value()) {
        std::cout << "Importing animations." << std::endl;
        for (const auto& animation : animations->animations) {
            auto name = animation.first;
            if (name.starts_with("animation."))
                name = name.substr(10);

            importAnimation(name, animation.second);
        }
    }

    std::cout << "Exporting model." << std::endl;

    auto exporter = FbxExporter::Create(manager, "");

#ifndef BINARY
    auto writerCount = manager->GetIOPluginRegistry()->GetWriterFormatCount();
    auto asciiIndex = 0;
    for (asciiIndex = 0; asciiIndex < writerCount; asciiIndex++) {
        if (manager->GetIOPluginRegistry()->WriterIsFBX(asciiIndex)) {
            std::string formatDesc(manager->GetIOPluginRegistry()->GetWriterFormatDescription(asciiIndex));
            if (formatDesc.find("ascii") != std::string::npos)
                break;
        }
    }

    auto status = exporter->Initialize(output, asciiIndex /*manager->GetIOPluginRegistry()->GetNativeWriterFormat()*/, manager->GetIOSettings());
#else
     auto status = exporter->Initialize(output, manager->GetIOPluginRegistry()->GetNativeWriterFormat(), manager->GetIOSettings());
#endif
    if (!status) {
        std::cerr << "Error: failed to initialize FbxExporter." << std::endl;
        return false;
    }

    exporter->Export(scene);
    exporter->Destroy();

    std::cout << "Export finished." << std::endl;

    return true;
}

bool FbxConverter::importMesh(const Badger::Mesh& meshData, size_t meshId) {
    auto positionsCount = meshData.positions.capacity();
    if (positionsCount != meshData.weights.capacity()) {
        std::cerr << "Error: Invalid mesh positions/weights detected." << std::endl;
        return false;
    }

    if (!meshData.indices.empty() && positionsCount != meshData.indices.capacity()) {
        std::cerr << "Error: Invalid mesh positions/indices detected." << std::endl;
        return false;
    }
    
    auto nodeName = "meshNode_" + std::to_string(meshId);
    auto meshName = "mesh_" + std::to_string(meshId);

    if (!meshData.name.empty()) {
        nodeName = nodeName.append("_" + meshData.name);
        meshName = meshName.append("_" + meshData.name);
    }

    FbxNode* meshNode = FbxNode::Create(scene, nodeName.c_str());
    FbxMesh* mesh = FbxMesh::Create(scene, meshName.c_str());
    meshNode->SetNodeAttribute(mesh);

    scene->GetRootNode()->AddChild(meshNode);

    std::cout << "Importing control points." << std::endl;
    mesh->InitControlPoints(meshData.positions.capacity());
    auto controlPoints = mesh->GetControlPoints();


    for (auto i = 0; i < meshData.positions.capacity(); i++) {
        auto pos = meshData.positions[i];
        controlPoints[i] = FbxVector4(pos[0], pos[1], pos[2]);
    }

    std::cout << "Importing polygons." << std::endl;
    for (int polygonIndex = 0; polygonIndex != meshData.triangles.capacity(); polygonIndex += 3) {
        mesh->BeginPolygon();
        mesh->AddPolygon(meshData.triangles[polygonIndex]);
        mesh->AddPolygon(meshData.triangles[polygonIndex + 1]);
        mesh->AddPolygon(meshData.triangles[polygonIndex + 2]);
        mesh->EndPolygon();
    }

    std::cout << "Importing normals." << std::endl;
    for (const auto& normalSet : meshData.normals) {
        std::cout << "Importing Normals Set." << std::endl;

        if (positionsCount != normalSet.capacity()) {
            std::cerr << "Error: Invalid normal set detected. " << std::endl;
            return false;
        }

        auto normalElement = mesh->CreateElementNormal();
        normalElement->SetMappingMode(FbxLayerElement::eByControlPoint);
        normalElement->SetReferenceMode(FbxLayerElement::eDirect);
        for (auto normal : normalSet) {
            normalElement->GetDirectArray().Add(FbxVector4(normal[0], normal[1], normal[2], normal[3]));
        }
    }

    std::cout << "Importing UVs." << std::endl;
    for (const auto& uvSet : meshData.uvs) {
        std::cout << "Importing UV Set." << std::endl;

        if (positionsCount != uvSet.capacity()) {
            std::cerr << "Error: Invalid UV set detected. " << std::endl;
            return false;
        }

        auto uvElement = mesh->CreateElementUV("UVs");
        uvElement->SetMappingMode(FbxLayerElement::eByControlPoint);
        uvElement->SetReferenceMode(FbxLayerElement::eDirect);
        for (auto uv : uvSet) {
            uvElement->GetDirectArray().Add(FbxVector2(uv[0], 1 - uv[1]));
        }
    }

    std::cout << "Importing colors." << std::endl;
    for (const auto& colorSet : meshData.colors) {
        std::cout << "Importing color set." << std::endl;

        if (positionsCount != colorSet.capacity()) {
            std::cerr << "Error: Invalid color set detected. " << std::endl;
            return false;
        }

        auto colorElement = mesh->CreateElementVertexColor();
        colorElement->SetMappingMode(FbxLayerElement::eByControlPoint);
        colorElement->SetReferenceMode(FbxLayerElement::eDirect);
        for (auto color : colorSet) {
            colorElement->GetDirectArray().Add(FbxColor(color[0], color[1], color[2], color[3]));
        }
    }

    if (!meshData.indices.empty()) {
        std::cout << "Assigning mesh to bones." << std::endl;
        auto skin = FbxSkin::Create(scene, (meshData.name + "_skin").c_str());

        std::unordered_map<std::string, FbxCluster*> clusterMap;

        for (auto i = 0; i < meshData.indices.capacity(); i++) {
            auto boneNames = meshData.indices[i];
            
            for (auto j = 0; j < boneNames.capacity(); j++) {
                auto boneName = boneNames[j];
                auto weight = meshData.weights[i][j];

                FbxCluster* cluster;
                if (auto pair = clusterMap.find(boneName); pair == clusterMap.end()) {
                    cluster = FbxCluster::Create(scene, (meshData.name + "_" + boneName + "_cluster").c_str());
                    auto skelNode = scene->GetRootNode()->FindChild(boneName.c_str());
                    cluster->SetLink(skelNode);
                    cluster->SetLinkMode(FbxCluster::eTotalOne);
                    clusterMap.insert({boneName, cluster});
                } else {
                    cluster = pair->second;
                }

                cluster->AddControlPointIndex(i, weight);
            }
        }

        auto transform = meshNode->EvaluateGlobalTransform();

        for (auto pair : clusterMap) {
            auto linkTransform = pair.second->GetLink()->EvaluateGlobalTransform();
            pair.second->SetTransformMatrix(transform);
            pair.second->SetTransformLinkMatrix(linkTransform);
            skin->AddCluster(pair.second);
        }

        mesh->AddDeformer(skin);
    }

    std::cout << "Importing material." << std::endl;
    auto materialLayer = mesh->CreateElementMaterial();
    materialLayer->SetMappingMode(FbxLayerElement::eAllSame);
    materialLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);
    materialLayer->GetIndexArray().Add(0);

    auto material = getMaterial(meshData.material);
    if (material == nullptr) {
        return false;
    }

    mesh->GetNode()->AddMaterial(material);

    mesh->GetNode()->SetShadingMode(FbxNode::eTextureShading);

    return true;
}

bool FbxConverter::importBone(const Badger::Bone& badgerBone) {
    auto isRootBone = badgerBone.parent.empty();
    auto skeletonNode = FbxNode::Create(scene, badgerBone.name.c_str());
    auto skeleton = FbxSkeleton::Create(scene, (badgerBone.name + "_bone").c_str());
    if (isRootBone) {
        skeleton->SetSkeletonType(FbxSkeleton::eRoot);
    } else {
        skeleton->SetSkeletonType(FbxSkeleton::eLimbNode);
    }

    skeletonNode->SetNodeAttribute(skeleton);
    //skeletonNode->SetNodeAttribute(skeleton);
    skeletonNode->SetRotationActive(true);
    skeletonNode->LclScaling.Set(FbxDouble3(badgerBone.scale[0], badgerBone.scale[1], badgerBone.scale[2]));
    //skeletonNode->ScalingPivot.Set(FbxDouble3(badgerBone.pivot[0], badgerBone.pivot[1], badgerBone.pivot[2]));
    //skeletonNode->RotationPivot.Set(FbxDouble3(badgerBone.pivot[0], badgerBone.pivot[1], badgerBone.pivot[2]));
    //skeletonNode->ScalingPivot.Set(FbxDouble3(badgerBone.pivot[0], badgerBone.pivot[1], badgerBone.pivot[2]));
    skeletonNode->LclTranslation.Set(FbxDouble3(badgerBone.pivot[0], badgerBone.pivot[1], badgerBone.pivot[2]));
    skeletonNode->LclRotation.Set(FbxDouble3(badgerBone.info.bindPoseRotation[0], badgerBone.info.bindPoseRotation[1], badgerBone.info.bindPoseRotation[2]));

    //skeletonNode->SetTransformationInheritType(FbxTransform::eInheritRSrs);

    if (!badgerBone.locators.empty()) {
        std::cout << "Importing locators." << std::endl;

        for (const auto& locatorPair : badgerBone.locators) {
            auto info = locatorPair.second;
            auto locator = FbxNull::Create(scene, (locatorPair.first + "_locator").c_str());
            auto locatorNode = FbxNode::Create(scene, locatorPair.first.c_str());
            
            locatorNode->SetNodeAttribute(locator);
            locatorNode->LclTranslation.Set(FbxDouble3(info.offset[0], info.offset[1], info.offset[2]));
            locatorNode->LclRotation.Set(FbxDouble3(info.rotation[0], info.rotation[1], info.rotation[2]));
            locatorNode->SetRotationActive(true);

            auto inheritType = info.discardScale 
                ? FbxTransform::eInheritRrs 
                : FbxTransform::eInheritRSrs;
            locatorNode->SetTransformationInheritType(inheritType);

            skeletonNode->AddChild(locatorNode);
        }
    }

    if (isRootBone) {
        scene->GetRootNode()->AddChild(skeletonNode);
    } else {
        auto parentNode = scene->GetRootNode()->FindChild(badgerBone.parent.c_str(), true);
        if (parentNode == nullptr) {
            std::cerr << "Error: could not find parent bone " << badgerBone.parent << "." << std::endl;
            return false; 
        }
        parentNode->AddChild(skeletonNode);
    }

    return true;
}

bool FbxConverter::importAnimation(const std::string& name, const Badger::Animation& badgerAnimation) {
    auto animationStack = FbxAnimStack::Create(scene, (name + "_stack").c_str());
    auto animationLayer = FbxAnimLayer::Create(scene, name.c_str());
    animationStack->AddMember(animationLayer);

    animationLayer->BlendMode.Set(FbxAnimLayer::eBlendAdditive);

    auto setKeyframes = [&](FbxPropertyT<FbxDouble3>& property, const std::unordered_map<double, Badger::AnimationProperty>& keyframeInfo) {
        if (keyframeInfo.empty())
            return;
        
        FbxTime keyframeTime;

        auto originalX = property.Get()[0];
        auto originalY = property.Get()[1];
        auto originalZ = property.Get()[2];

        auto curveNode = property.GetCurveNode(animationLayer, true);
        auto translationX = property.GetCurve(animationLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
        auto translationY = property.GetCurve(animationLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
        auto translationZ = property.GetCurve(animationLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

        translationX->KeyModifyBegin();
        translationY->KeyModifyBegin();
        translationZ->KeyModifyBegin();

        auto ind = 0;
        for (const auto& currentKeyframe : keyframeInfo) {
            keyframeTime.SetSecondDouble(currentKeyframe.first);
            
            ind = translationX->KeyAdd(keyframeTime);
            translationX->KeySet(
                ind,
                keyframeTime, 
                originalX + currentKeyframe.second.post[0],
                FbxAnimCurveDef::eInterpolationLinear
            );

            ind = translationY->KeyAdd(keyframeTime);
            translationY->KeySet(
                ind,
                keyframeTime, 
                originalY + currentKeyframe.second.post[1],
                FbxAnimCurveDef::eInterpolationLinear
            );

            ind = translationZ->KeyAdd(keyframeTime);
            translationZ->KeySet(
                ind,
                keyframeTime, 
                originalZ + currentKeyframe.second.post[2],
                FbxAnimCurveDef::eInterpolationLinear
            );
        }

        translationX->KeyModifyEnd();
        translationY->KeyModifyEnd();
        translationZ->KeyModifyEnd();
    };
    
    for (const auto& boneAnimation : badgerAnimation.bones) {
        auto boneNode = scene->FindNodeByName(boneAnimation.first.c_str());
        if (boneNode == nullptr) {
            std::cerr << "Error: could not find bone " << boneAnimation.first << " in the scene." << std::endl;
            return false;
        }

        setKeyframes(boneNode->LclTranslation, boneAnimation.second.position);
        setKeyframes(boneNode->LclRotation, boneAnimation.second.rotation);
    }

    return true;
}

FbxSurfaceMaterial* FbxConverter::getMaterial(const std::string& name) {
    if (auto it = createdMaterials.find(name); it != createdMaterials.end())
        return it->second;

    std::cout << "Importing material " << name << "." << std::endl;

    auto materialData = loader->getMaterial(name);
    if (!materialData) {
        return nullptr;
    }

    auto materialInfo = materialData->info.textures;

    auto useColorMap = !materialInfo.diffuse.empty();
    auto useNormalMap = !materialInfo.normal.empty();
    auto useCoefficientMap = !materialInfo.coeff.empty();
    auto useEmissiveMap = !materialInfo.emissive.empty();

    // Creating properties for Stingray PBS material
    // Done this way to ensure same order as in a Maya export

    auto mayaMatName = materialData->baseName.empty() ? materialData->name : (materialData->name + "___" + materialData->baseName);

    auto material = FbxSurfaceMaterial::Create(scene, mayaMatName.c_str());
    auto mayaProperty = FbxProperty::Create(material, FbxCompoundDT, "Maya", "");

    auto typeIdProp = FbxProperty::Create(mayaProperty, FbxIntDT, "TypeId", "");
    typeIdProp.Set(1166017);

    auto gdCubeProp = FbxProperty::Create(mayaProperty, FbxDouble3DT, "TEX_global_diffuse_cube", "");
    auto gsCubeProp = FbxProperty::Create(mayaProperty, FbxDouble3DT, "TEX_global_specular_cube", "");
    auto brdfLutProp = FbxProperty::Create(mayaProperty, FbxDouble3DT, "TEX_brdf_lut", "");

    auto useNormalMapProp = FbxProperty::Create(mayaProperty, FbxFloatDT, "use_normal_map", "");
    useNormalMapProp.Set(useNormalMap);
    auto uvOffsetProp = FbxProperty::Create(mayaProperty, FbxDouble2DT, "uv_offset", "");
    auto uvScaleProp = FbxProperty::Create(mayaProperty, FbxDouble2DT, "uv_scale", "");
    uvScaleProp.Set(FbxVector2(1, 1));
    auto normalMapProp = FbxProperty::Create(mayaProperty, FbxDouble3DT, "TEX_normal_map", "");

    auto useColorMapProp = FbxProperty::Create(mayaProperty, FbxFloatDT, "use_color_map", "");
    useColorMapProp.Set(useColorMap);
    auto colorMapProp = FbxProperty::Create(mayaProperty, FbxDouble3DT, "TEX_color_map", "");
    auto baseColorProp = FbxProperty::Create(mayaProperty, FbxDouble3DT, "base_color", "");

    auto useMetallicMapProp = FbxProperty::Create(mayaProperty, FbxFloatDT, "use_metallic_map", "");
    useMetallicMapProp.Set(useCoefficientMap);
    auto metallicMapProp = FbxProperty::Create(mayaProperty, FbxDouble3DT, "TEX_metallic_map", "");
    auto metallicProp = FbxProperty::Create(mayaProperty, FbxFloatDT, "metallic", "");

    auto useRoughnessMapProp = FbxProperty::Create(mayaProperty, FbxFloatDT, "use_roughness_map", "");
    useRoughnessMapProp.Set(useCoefficientMap);
    auto roughnessMapProp = FbxProperty::Create(mayaProperty, FbxDouble3DT, "TEX_roughness_map", "");
    auto roughnessProp = FbxProperty::Create(mayaProperty, FbxFloatDT, "roughness", "");

    auto useEmissiveMapProp = FbxProperty::Create(mayaProperty, FbxFloatDT, "use_emissive_map", "");
    useEmissiveMapProp.Set(useEmissiveMap);
    auto emissiveMapProp = FbxProperty::Create(mayaProperty, FbxDouble3DT, "TEX_emissive_map", "");
    auto emissiveProp = FbxProperty::Create(mayaProperty, FbxDouble3DT, "emissive", "");
    auto emissiveIntensityProp = FbxProperty::Create(mayaProperty, FbxFloatDT, "emissive_intensity", "");
    emissiveIntensityProp.Set(1);

    auto useAoProp = FbxProperty::Create(mayaProperty, FbxFloatDT, "use_ao_map", "");
    useAoProp.Set(false);
    auto texAoProp = FbxProperty::Create(mayaProperty, FbxDouble3DT, "TEX_ao_map", "");

    // Assigning textures to material

    auto createTexture = [&](const std::string& texName, const std::string& texPath) {
        auto tex = FbxFileTexture::Create(scene, texName.c_str());
        tex->SetFileName(texPath.c_str());
        tex->SetTextureUse(FbxTexture::eStandard);
        tex->SetMappingType(FbxTexture::eUV);
        tex->SetMaterialUse(FbxFileTexture::eModelMaterial);
        tex->SetSwapUV(false);
        tex->SetTranslation(0.0, 0.0);
        tex->SetScale(1.0, 1.0);
        tex->SetRotation(0.0, 0.0);
        //tex->UVSet.Set("UVs");
        return tex;
    };

    if (useColorMap) {
        std::cout << "Adding diffuse texture." << std::endl;

        auto tex = createTexture(name + "_diffuse", materialInfo.diffuse + ".png");
        tex->ConnectDstProperty(colorMapProp);
    }

    if (useNormalMap) {
        std::cout << "Adding normal texture." << std::endl;

        auto tex = createTexture(name + "_normal", materialInfo.normal + ".png");
        tex->ConnectDstProperty(normalMapProp);
    }

    if (useCoefficientMap) {
        std::cout << "Adding coefficient texture." << std::endl;

        auto tex = createTexture(name + "_coeff", materialInfo.coeff + ".png");
        tex->ConnectDstProperty(metallicMapProp);
        tex->ConnectDstProperty(roughnessMapProp);
    }

    if (useEmissiveMap) {
        std::cout << "Adding emissive texture." << std::endl;

        auto tex = createTexture(name + "_emissive", materialInfo.emissive + ".hdr");
        tex->ConnectDstProperty(emissiveMapProp);
    }

    if (pbShaderImplementation == nullptr && !generateShaderImplementation(mayaProperty)) {
        return nullptr;
    }

    material->AddImplementation(pbShaderImplementation);
    material->SetDefaultImplementation(pbShaderImplementation);

    createdMaterials.insert({name, material});
    return material;
}

bool FbxConverter::generateShaderImplementation(const FbxProperty& mayaProp) {
    std::cout << "Generating default shader implementation." << std::endl;

    pbShaderImplementation = FbxImplementation::Create(scene, "PBS_Implementation");
    pbShaderImplementation->RenderAPI = "SFX_PBS_SHADER";
    pbShaderImplementation->Language = "SFX";
    pbShaderImplementation->LanguageVersion = "28";
    auto shaderGraphProp = FbxProperty::Create(pbShaderImplementation, FbxBlobDT, "ShaderGraph", "");
    
    std::cout << "Decompressing shadergraph." << std::endl;

    // Decompress shadergraph
    size_t decompressedLength = DEFAULT_SFX_SHADERGRAPH_DECOMPRESSED_LENGTH;
    auto decompressedData = new unsigned char[decompressedLength];
    auto result = BrotliDecoderDecompress(
        DEFAULT_SFX_SHADERGRAPH_COMPRESSED_LENGTH,
        DEFAULT_SFX_SHADERGRAPH_COMPRESSED, 
        &decompressedLength, 
        decompressedData
    );

    if (result != BrotliDecoderResult::BROTLI_DECODER_RESULT_SUCCESS) {
        delete[] decompressedData;
        std::cerr << "Error: failed to decompress shadergraph binary. Result: " << result << std::endl;
        return false; 
    }
    
    shaderGraphProp.Set(FbxBlob(decompressedData, decompressedLength));
    delete[] decompressedData;

    auto bindingTable = pbShaderImplementation->AddNewTable("root", "shader");
    pbShaderImplementation->RootBindingName = "root";

    auto descendant = mayaProp.GetFirstDescendent();
    while (descendant.IsValid()) {
        FbxBindingTableEntry& tableEntry = bindingTable->AddNewEntry();
        FbxPropertyEntryView source(&tableEntry, true, true);
        source.SetProperty(descendant.GetHierarchicalName());
        FbxSemanticEntryView destination(&tableEntry, false, true);
        destination.SetSemantic(descendant.GetName());
        descendant = mayaProp.GetNextDescendent(descendant);
    }

    return true;
}