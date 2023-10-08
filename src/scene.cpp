#include <iostream>
#include "scene.h"
#include <cstring>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/string_cast.hpp>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "gltf/tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define DEBUG_GLTF 1

Scene::Scene(string filename) {
    basePath = filename.substr(0, filename.rfind('/') + 1);
    cout << "Reading scene from " << filename << " ..." << endl;
    cout << " " << endl;
    char* fname = (char*)filename.c_str();
    fp_in.open(fname);
    if (!fp_in.is_open()) {
        cout << "Error reading from file - aborting!" << endl;
        throw;
    }
    while (fp_in.good()) {
        string line;
        utilityCore::safeGetline(fp_in, line);
        if (!line.empty()) {
            vector<string> tokens = utilityCore::tokenizeString(line);
            if (strcmp(tokens[0].c_str(), "MATERIAL") == 0) {
                loadMaterial(tokens[1]);
                cout << " " << endl;
            }
            else if (strcmp(tokens[0].c_str(), "OBJECT") == 0) {
                loadGeom(tokens[1]);
                cout << " " << endl;
            }
            else if (strcmp(tokens[0].c_str(), "CAMERA") == 0) {
                loadCamera();
                cout << " " << endl;
            }
        }
    }
}

int Scene::loadGeom(string objectid) {
    int id = atoi(objectid.c_str());
    if (id != geoms.size()) {
        cout << "ERROR: OBJECT ID does not match expected number of geoms" << endl;
        return -1;
    }
    else {
        cout << "Loading Geom " << id << "..." << endl;
        Geom newGeom;
        string line;

        //load object type
        utilityCore::safeGetline(fp_in, line);
        if (!line.empty() && fp_in.good()) {
            vector<string> tokens = utilityCore::tokenizeString(line);
            if (strcmp(tokens[0].c_str(), "sphere") == 0) {
                cout << "Creating new sphere..." << endl;
                newGeom.type = SPHERE;
            } 
            else if (strcmp(tokens[0].c_str(), "cube") == 0) {
                cout << "Creating new cube..." << endl;
                newGeom.type = CUBE;
            }
            else if (strcmp(tokens[0].c_str(), "gltf") == 0) {
                cout << "Creating new gltf mesh..." << endl;
                newGeom.type = GLTF;
                loadMeshGltf(basePath + tokens[1], newGeom, id);
                return 1;
            }
            else if (strcmp(tokens[0].c_str(), "obj") == 0) {
                cout << "Creating new obj mesh..." << endl;
                newGeom.type = OBJ;                
                if (!loadMeshObj(basePath + tokens[1], newGeom)) {
                    return -1;
                }
            }
        }

        //link material
        utilityCore::safeGetline(fp_in, line);
        if (!line.empty() && fp_in.good()) {
            vector<string> tokens = utilityCore::tokenizeString(line);
            newGeom.materialid = atoi(tokens[1].c_str());
            cout << "Connecting Geom " << objectid << " to Material " << newGeom.materialid << "..." << endl;
        }

        //load transformations
        utilityCore::safeGetline(fp_in, line);
        while (!line.empty() && fp_in.good()) {
            vector<string> tokens = utilityCore::tokenizeString(line);

            //load tranformations
            if (strcmp(tokens[0].c_str(), "TRANS") == 0) {
                newGeom.translation = glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
            } else if (strcmp(tokens[0].c_str(), "ROTAT") == 0) {
                newGeom.rotation = glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
            } else if (strcmp(tokens[0].c_str(), "SCALE") == 0) {
                newGeom.scale = glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
            } else if (strcmp(tokens[0].c_str(), "VEL") == 0) {
                newGeom.velocity = glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
            }

            utilityCore::safeGetline(fp_in, line);
        }

        newGeom.transform = utilityCore::buildTransformationMatrix(
                newGeom.translation, newGeom.rotation, newGeom.scale);
        newGeom.inverseTransform = glm::inverse(newGeom.transform);
        newGeom.invTranspose = glm::inverseTranspose(newGeom.transform);

        geoms.push_back(newGeom);
        return 1;
    }
}

int Scene::loadCamera() {
    cout << "Loading Camera ..." << endl;
    RenderState &state = this->state;
    Camera &camera = state.camera;
    float fovy;

    //load static properties
    for (int i = 0; i < 5; i++) {
        string line;
        utilityCore::safeGetline(fp_in, line);
        vector<string> tokens = utilityCore::tokenizeString(line);
        if (strcmp(tokens[0].c_str(), "RES") == 0) {
            camera.resolution.x = atoi(tokens[1].c_str());
            camera.resolution.y = atoi(tokens[2].c_str());
        } else if (strcmp(tokens[0].c_str(), "FOVY") == 0) {
            fovy = atof(tokens[1].c_str());
        } else if (strcmp(tokens[0].c_str(), "ITERATIONS") == 0) {
            state.iterations = atoi(tokens[1].c_str());
        } else if (strcmp(tokens[0].c_str(), "DEPTH") == 0) {
            state.traceDepth = atoi(tokens[1].c_str());
        } else if (strcmp(tokens[0].c_str(), "FILE") == 0) {
            state.imageName = tokens[1];
        }
    }

    string line;
    utilityCore::safeGetline(fp_in, line);
    while (!line.empty() && fp_in.good()) {
        vector<string> tokens = utilityCore::tokenizeString(line);
        if (strcmp(tokens[0].c_str(), "EYE") == 0) {
            camera.position = glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
        } else if (strcmp(tokens[0].c_str(), "LOOKAT") == 0) {
            camera.lookAt = glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
        } else if (strcmp(tokens[0].c_str(), "UP") == 0) {
            camera.up = glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
        }

        utilityCore::safeGetline(fp_in, line);
    }

    //calculate fov based on resolution
    float yscaled = tan(fovy * (PI / 180));
    float xscaled = (yscaled * camera.resolution.x) / camera.resolution.y;
    float fovx = (atan(xscaled) * 180) / PI;
    camera.fov = glm::vec2(fovx, fovy);

    camera.right = glm::normalize(glm::cross(camera.view, camera.up));
    camera.pixelLength = glm::vec2(2 * xscaled / (float)camera.resolution.x,
                                   2 * yscaled / (float)camera.resolution.y);

    camera.view = glm::normalize(camera.lookAt - camera.position);

    //set up render camera stuff
    int arraylen = camera.resolution.x * camera.resolution.y;
    state.image.resize(arraylen);
    std::fill(state.image.begin(), state.image.end(), glm::vec3());

    cout << "Loaded camera!" << endl;
    return 1;
}

int Scene::loadMaterial(string materialid) {
    int id = atoi(materialid.c_str());
    if (id != materials.size()) {
        cout << "ERROR: MATERIAL ID does not match expected number of materials" << endl;
        return -1;
    } else {
        cout << "Loading Material " << id << "..." << endl;
        Material newMaterial;

        // ------------------------------------------------------------------------
        // REMEMBER to change i in the loop if adding new properties!!!!!!
        // ------------------------------------------------------------------------
        //load static properties
        for (int i = 0; i < 8; i++) {
            string line;
            utilityCore::safeGetline(fp_in, line);
            vector<string> tokens = utilityCore::tokenizeString(line);
            if (strcmp(tokens[0].c_str(), "RGB") == 0) {
                glm::vec3 color( atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()) );
                newMaterial.color = color;
            } else if (strcmp(tokens[0].c_str(), "SPECEX") == 0) {
                newMaterial.specular.exponent = atof(tokens[1].c_str());
            } else if (strcmp(tokens[0].c_str(), "SPECRGB") == 0) {
                glm::vec3 specColor(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
                newMaterial.specular.color = specColor;
            } else if (strcmp(tokens[0].c_str(), "REFL") == 0) {
                newMaterial.hasReflective = atof(tokens[1].c_str());
            } else if (strcmp(tokens[0].c_str(), "REFR") == 0) {
                newMaterial.hasRefractive = atof(tokens[1].c_str());
            } else if (strcmp(tokens[0].c_str(), "REFRIOR") == 0) {
                newMaterial.indexOfRefraction = atof(tokens[1].c_str());
            } else if (strcmp(tokens[0].c_str(), "EMITTANCE") == 0) {
                newMaterial.emittance = atof(tokens[1].c_str());
            } else if (strcmp(tokens[0].c_str(), "TRANS") == 0) {
                newMaterial.hasTransmission = atof(tokens[1].c_str());
            }
        }
        materials.push_back(newMaterial);
        return 1;
    }
}

//-------------------------------------------------------------------
//----------Helper functions for gltf/glb mesh loading---------------
//-------------------------------------------------------------------
// 
// Tutorials: https://github.com/syoyo/tinygltf/tree/release/examples/basic
bool loadModel(tinygltf::Model& model, const char* filename) {
    tinygltf::TinyGLTF loader;
    string err;
    string warn;
    bool res;

    if (utilityCore::fileHasExtension(filename, ".gltf")) {
        res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
    }
    else if (utilityCore::fileHasExtension(filename, ".glb")) {
        res = loader.LoadBinaryFromFile(&model, &err, &warn, filename); // for binary glTF(.glb)
    }
    else {
        printf("ERROR: File extension not supported\n");
        return -1;
    }

    if (!warn.empty()) {
        std::cout << "WARN: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cout << "ERR: " << err << std::endl;
    }

    if (!res)
        std::cout << "Failed to load glTF/glb: " << filename << std::endl;
    else
        std::cout << "Loaded glTF/glb: " << filename << std::endl;

    return res;
}

// print vert pos and nor in a triangle
inline void printTri(Triangle& tri)
{
    printf("v0 pos: %f %f %f\n", tri.v0.pos.x, tri.v0.pos.y, tri.v0.pos.z);
    printf("v1 pos: %f %f %f\n", tri.v1.pos.x, tri.v1.pos.y, tri.v1.pos.z);
    printf("v2 pos: %f %f %f\n\n", tri.v2.pos.x, tri.v2.pos.y, tri.v2.pos.z);

    printf("v0 nor: %f %f %f\n", tri.v0.nor.x, tri.v0.nor.y, tri.v0.nor.z);
    printf("v1 nor: %f %f %f\n", tri.v1.nor.x, tri.v1.nor.y, tri.v1.nor.z);
    printf("v2 nor: %f %f %f\n", tri.v2.nor.x, tri.v2.nor.y, tri.v2.nor.z);
}

// glTF structure: https://github.com/KhronosGroup/glTF-Tutorials/blob/master/gltfTutorial/gltfTutorial_003_MinimalGltfFile.md
void parseMesh(tinygltf::Model& model, tinygltf::Mesh& mesh, int& triCount, std::vector<Triangle>& tris) {
    for (size_t i = 0; i < mesh.primitives.size(); ++i) {
        tinygltf::Primitive& primitive = mesh.primitives[i];

        const float* posData = NULL;
        const float* norData = NULL;
        const uint16_t* indexData16 = NULL;
        const uint32_t* indexData32 = NULL;

        // parse position buffer data
        const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
        const size_t posElemCount = posAccessor.count;
        cout << "posAccessor.count: " << posAccessor.count << endl;
        const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
        const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];
        posData = reinterpret_cast<const float*>(&(posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset])); // data alignment
        
        // parse normal buffer data, if exists
        if (primitive.attributes.at("NORMAL") >= 0) {
            const tinygltf::Accessor& norAccessor = model.accessors[primitive.attributes.at("NORMAL")];
            const tinygltf::BufferView& norBufferView = model.bufferViews[norAccessor.bufferView];
            const tinygltf::Buffer& norBuffer = model.buffers[norBufferView.buffer];
            norData = reinterpret_cast<const float*>(&(norBuffer.data[norBufferView.byteOffset + norAccessor.byteOffset]));
        }

        // TODO: uv

        // The mesh primitive describes an indexed geometry, which is indicated by the indices property.
        // By default, it is assumed to describe a set of triangles, so that three consecutive indices
        // are the indices of the vertices of one triangle.
        Triangle tri;
        if (primitive.indices >= 0)  {
            // triangulate mesh
            const tinygltf::Accessor& idxAccessor = model.accessors[primitive.indices];
            const tinygltf::BufferView& idxBufferView = model.bufferViews[idxAccessor.bufferView];
            const tinygltf::Buffer& idxBuffer = model.buffers[idxBufferView.buffer];
            indexData16 = reinterpret_cast<const uint16_t*>(&(idxBuffer.data[idxBufferView.byteOffset + idxAccessor.byteOffset]));
            indexData32 = reinterpret_cast<const uint32_t*>(&(idxBuffer.data[idxBufferView.byteOffset + idxAccessor.byteOffset]));

            // populate Triangle structure
            cout << "idxAccessor.count: " << idxAccessor.count << endl;
            for (size_t i = 0; i < idxAccessor.count; i += 3) {
                // vertex positions
                int vertIdx0, vertIdx1, vertIdx2;
                if (idxAccessor.componentType == 5123) {// unsigned_short
                    vertIdx0 = indexData16[i];
                    vertIdx1 = indexData16[i + 1];
                    vertIdx2 = indexData16[i + 2];
                }
                else { // defualt to 5126, float
                    vertIdx0 = indexData32[i];
                    vertIdx1 = indexData32[i + 1];
                    vertIdx2 = indexData32[i + 2];
                }

                tri.v0.pos = glm::vec3(posData[vertIdx0 * 3], posData[vertIdx0 * 3 + 1], posData[vertIdx0 * 3 + 2]);
                tri.v1.pos = glm::vec3(posData[vertIdx1 * 3], posData[vertIdx1 * 3 + 1], posData[vertIdx1 * 3 + 2]);
                tri.v2.pos = glm::vec3(posData[vertIdx2 * 3], posData[vertIdx2 * 3 + 1], posData[vertIdx2 * 3 + 2]);

                // vertex normals
                if (norData) {
                    tri.v0.nor = glm::vec3(norData[vertIdx0 * 3], norData[vertIdx0 * 3 + 1], norData[vertIdx0 * 3 + 2]);
                    tri.v1.nor = glm::vec3(norData[vertIdx1 * 3], norData[vertIdx1 * 3 + 1], norData[vertIdx1 * 3 + 2]);
                    tri.v2.nor = glm::vec3(norData[vertIdx2 * 3], norData[vertIdx2 * 3 + 1], norData[vertIdx2 * 3 + 2]);
                }

                // TODO: uv
            }
        }
        // not using indices
        else {
            for (size_t i = 0; i < posElemCount; i += 3) {
                // vertex positions
                int vertIdx0 = i;
                int vertIdx1 = i + 1;
                int vertIdx2 = i + 2;

                tri.v0.pos = glm::vec3(posData[vertIdx0 * 3], posData[vertIdx0 * 3 + 1], posData[vertIdx0 * 3 + 2]);
                tri.v1.pos = glm::vec3(posData[vertIdx1 * 3], posData[vertIdx1 * 3 + 1], posData[vertIdx1 * 3 + 2]);
                tri.v2.pos = glm::vec3(posData[vertIdx2 * 3], posData[vertIdx2 * 3 + 1], posData[vertIdx2 * 3 + 2]);

                // vertex normals
                if (norData) {
                    tri.v0.nor = glm::vec3(norData[vertIdx0 * 3], norData[vertIdx0 * 3 + 1], norData[vertIdx0 * 3 + 2]);
                    tri.v1.nor = glm::vec3(norData[vertIdx1 * 3], norData[vertIdx1 * 3 + 1], norData[vertIdx1 * 3 + 2]);
                    tri.v2.nor = glm::vec3(norData[vertIdx2 * 3], norData[vertIdx2 * 3 + 1], norData[vertIdx2 * 3 + 2]);
                }

                // TODO: uv
            }
        }

        tri.computeCentroid();
        tris.push_back(tri);
        triCount++;
#if DEBUG_GLTF
        cout << "Triangle count: " << triCount << endl;
        printTri(tri);
#endif
    }
}

// recursively load node and children nodes of model
void parseModelNodes(tinygltf::Model& model, tinygltf::Node& node, int& triCount, std::vector<Triangle>& tris) {
    if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
        parseMesh(model, model.meshes[node.mesh], triCount, tris);
    }
    for (size_t i = 0; i < node.children.size(); i++) {
        //assert((node.children[i] >= 0) && (node.children[i] < model.nodes.size()));
        parseModelNodes(model, model.nodes[node.children[i]], triCount, tris);
    }
}
void parseModel(tinygltf::Model& model, int& triCount, std::vector<Triangle>& tris) {
    const tinygltf::Scene& scene = model.scenes[model.defaultScene];
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
#if DEBUG_GLTF
        //assert((scene.nodes[i] >= 0) && (scene.nodes[i] < model.nodes.size()));
        cout << "model node: " << scene.nodes[i] << endl;
#endif
        parseModelNodes(model, model.nodes[scene.nodes[i]], triCount, tris);
    }
}

int Scene::loadMeshGltf(string filename, Geom& geom, int objectId) {
    cout << "Loading glTF Mesh: " << filename << endl;
    tinygltf::Model model;
    meshCount = 0;
    
    auto startTime = utilityCore::timeSinceEpochMillisec();
    // read model file
    if (!loadModel(model, filename.c_str())) return -1;
    auto endTime = utilityCore::timeSinceEpochMillisec();
    auto duration = (endTime - startTime);
    cout << "Reading mesh file took: " << duration << "ms" << endl;

    geom.startTriIdx = triangles.size();
    geom.triangleCount = 0;

    startTime = utilityCore::timeSinceEpochMillisec();
    // parse mesh structure
    parseModel(model, geom.triangleCount, triangles);
    endTime = utilityCore::timeSinceEpochMillisec();
    duration = (endTime - startTime);
    cout << "Parsing mesh took: " << duration << "ms" << endl;

    meshCount++;
    return 1;
}

// Tutorial: https://github.com/tinyobjloader/tinyobjloader/tree/release/examples/viewer
int Scene::loadMeshObj(string filename, Geom& geom) {
    cout << "Loading OBJ Mesh: " << filename << endl;

    tinyobj::ObjReaderConfig readerConfig;
    tinyobj::ObjReader reader;

    // load obj file
    if (!reader.ParseFromFile(filename, readerConfig)) {
        if (!reader.Error().empty()) {
            cerr << "TinyObjReader: " << reader.Error();
        }
        return -1;
    }

    if (!reader.Warning().empty()) {
        cout << "TinyObjReader: " << reader.Warning();
        return -1;
    }

    const tinyobj::attrib_t& attrib = reader.GetAttrib();
    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
    const std::vector<tinyobj::material_t>& materials= reader.GetMaterials();

    meshCount = 0;
    geom.startTriIdx = triangles.size();

    // traverse mesh structure
    for (size_t s = 0; s < shapes.size(); s++) {
        std::vector<Triangle> trisInMesh;

        // track AABB for each triangle
        glm::vec3 bbMin = glm::vec3(FLT_MAX);
        glm::vec3 bbMax = glm::vec3(-FLT_MAX);

        // traverse polygons
        size_t idxOffset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            Triangle tri;

            int i = 0;
            size_t numVert = shapes[s].mesh.num_face_vertices[f]; // current face's vertex count
            for (size_t v = 0; v < numVert; v++) {
                tinyobj::index_t idx = shapes[s].mesh.indices[idxOffset + v];

                // get vertex positions
                glm::vec3 vertPos = glm::vec3(
                    attrib.vertices[size_t(idx.vertex_index) * 3],
                    attrib.vertices[size_t(idx.vertex_index) * 3 + 1],
                    attrib.vertices[size_t(idx.vertex_index) * 3 + 2]
                );
                if (i%3 == 0) tri.v0.pos = vertPos;
                if (i%3 == 1) tri.v1.pos = vertPos;
                if (i%3 == 2) tri.v2.pos = vertPos;

                // get vertex normals
                if (idx.normal_index >= 0) {
                    glm::vec3 vertNor = glm::vec3(
                        attrib.normals[size_t(idx.normal_index) * 3],
                        attrib.normals[size_t(idx.normal_index) * 3 + 1],
                        attrib.normals[size_t(idx.normal_index) * 3 + 2]
                    );
                    if (i % 3 == 0) tri.v0.nor = vertNor;
                    if (i % 3 == 1) tri.v1.nor = vertNor;
                    if (i % 3 == 2) tri.v2.nor = vertNor;
                }
                i++;

                // update AABB
                bbMin = glm::min(bbMin, vertPos);
                bbMax = glm::max(bbMax, vertPos);
            }
            idxOffset += numVert;

            tri.computeAABB();
            tri.computeCentroid();
            tri.objectId = f;
            trisInMesh.push_back(tri);
        }

        // initialize new geom
        geom.aabb.min = bbMin;
        geom.aabb.max = bbMax;
        geom.startTriIdx = triangles.size();
        geom.triangleCount = trisInMesh.size();
        
        // update scene attributes
        triangles.insert(triangles.end(), trisInMesh.begin(), trisInMesh.end());
        meshCount++;
    }
    return 1;
}