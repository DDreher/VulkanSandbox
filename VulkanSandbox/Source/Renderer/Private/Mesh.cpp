#include "Mesh.h"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "FileIO.h"

SharedPtr<MeshData> MeshData::Load(const String& asset_path)
{
    CHECK_MSG(asset_path.empty() == false, "Failed to load mesh from file: Empty path", asset_path);

    MeshData mesh_data;

    Assimp::Importer importer;

    Array<char> bytes = FileIO::ReadFile(asset_path);

    uint32 importer_flags = aiProcessPreset_TargetRealtime_Fast;
    const aiScene* scene = importer.ReadFileFromMemory(bytes.data(), bytes.size(), importer_flags);
    CHECK_MSG(scene != nullptr, "Failed to load mesh from file: {}", asset_path);

    for (uint32 mesh_idx = 0; mesh_idx < scene->mNumMeshes; ++mesh_idx)
    {
        aiMesh* mesh = scene->mMeshes[mesh_idx];
        for (uint32 vertex_id = 0; vertex_id < mesh->mNumVertices; ++vertex_id)
        {
            Vertex v; 
            const aiVector3D& pos = mesh->mVertices[vertex_id];
            v.pos_ = { pos.x, pos.y, pos.z };

            //const aiVector3D& normal = mesh->HasNormals() ? mesh->mNormals[vertex_id] : aiVector3D(0.0f, 0.0f, 0.0f);
            //mesh_data.normals_.push_back({ normal.x, normal.y, normal.z });

            const aiVector3D& uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][vertex_id] : aiVector3D(0.0f, 0.0f, 0.0f);
            v.tex_coords_ = { uv.x, 1.0f - uv.y };  // 0 is at the top...

            const aiColor4D& color = mesh->HasVertexColors(0) ? mesh->mColors[0][vertex_id] : aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
            v.color_ = {color.r, color.g, color.b};

            mesh_data.vertices_.push_back(v);
        }

        for (uint32 i = 0; i < mesh->mNumFaces; ++i)
        {
            const aiFace& face = mesh->mFaces[i];
            CHECK(face.mNumIndices == 3);
            mesh_data.indices_.push_back(face.mIndices[0]);
            mesh_data.indices_.push_back(face.mIndices[1]);
            mesh_data.indices_.push_back(face.mIndices[2]);
        }
    }

    CHECK(mesh_data.indices_.size() % 3 == 0);
    return MakeShared<MeshData>(mesh_data);
}

Mesh::Mesh()
{

}

Mesh::Mesh(const Vec3& position, float scale, const String& asset_path)
{
    mesh_data_ = MeshData::Load(asset_path);
    transform_ = Mat4(1.0f);
}

Mesh::Mesh(const Vec3& position, float scale, SharedPtr<MeshData> mesh_data)
    : mesh_data_(mesh_data)
{
    transform_ = Mat4(1.0f);
}
