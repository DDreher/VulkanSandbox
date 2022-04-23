#pragma once
#include "Vertex.h"

struct MeshData
{
    static SharedPtr<MeshData> Load(const String& asset_path);

    Array<uint32> indices_;
    Array<Vertex> vertices_;
};

class Mesh
{
public:
    Mesh();
    Mesh(const Vec3& position, float scale, const String& asset_path);
    Mesh(const Vec3& position, float scale, SharedPtr<MeshData> mesh_data);

    const MeshData& GetMeshData() const { return *mesh_data_; }
    
    const Mat4& GetTransform() const { return transform_; }
    void SetTransform(const Mat4& m) { transform_ = m; }
private:
    SharedPtr<MeshData> mesh_data_;
    Mat4 transform_ = glm::identity<glm::mat4>();
};
