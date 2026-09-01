#include "voxeldesigner.h"
#include <nvh/gltfscene.cpp>
//#include "autogen/VoxelDesigner.comp.h"
// using BufferT = nanovdb::HostBuffer;

void VoxelDesigner::loadModel(ShapeType shapeType, Shape shape, float stepSize)
{
    // 填充m_voxelModel内部的aabbs
    switch (shapeType)
    {
    case ShapeType::CUBE:
        createCube(shape.length, shape.width, shape.height, stepSize);
        break;
    case ShapeType::ELLIPSOID:
        createEllipsoid(shape.height, shape.width, stepSize);
        break;
    case ShapeType::PLANE:
        createBackground(shape.length, shape.width, stepSize);
        break;
    default:
        createCube(shape.length, shape.width, shape.height, stepSize);
        break;
    }
}

// 均为xyz坐标
std::vector<glm::ivec3> VoxelDesigner::createEllipsoid(float height, float width, float stepSize) // Shape.length...
{
    std::vector<glm::ivec3> discreteCoord;
    std::vector<Aabb> aabbs;

    int widthSize = std::ceil(width / stepSize);
    int heightSize = std::ceil(height / stepSize);
    int lengthSize = std::ceil(width / stepSize);

    float widthCenter = widthSize / 2.0;
    float heightCenter = heightSize / 2.0;
    float lengthCenter = lengthSize / 2.0;

    for (int i = 0; i < lengthSize; i++)
    {
        for (int j = 0; j < widthSize; j++)
        {
            for (int k = 0; k < heightSize; k++)
            {
                // move to center of 1x1 voxel
                // (coord + 0.5) - (center + 0.5) = coord - center
                float dist = (i - lengthCenter) * (i - lengthCenter) / (lengthCenter * lengthCenter) +
                             (j - widthCenter) * (j - widthCenter) / (widthCenter * widthCenter) +
                             (k - heightCenter) * (k - heightCenter) / (heightCenter * heightCenter);
                if (dist <= 1.0)
                {
                    // ellipsoid.activedNum++;
                    glm::vec3 coord = {i, j, k};
                    Aabb aabb = { {coord},
                                 {coord + glm::vec3(1)}};
                    discreteCoord.emplace_back(coord);
                    aabbs.emplace_back(aabb);
                }
            }
        }
    }

    /*if (ellipsoid.activedNum < 1)
    {
        int point[3] = {int(widthCenter), int(widthCenter), int(widthCenter)};
        ellipsoid.voxels.emplace_back(point);
        ellipsoid.activedNum++;
    }*/

    m_voxelModel.centerPoints = discreteCoord;
    m_voxelModel.aabbs = aabbs;
    return discreteCoord;
}


std::vector<glm::ivec3> VoxelDesigner::createCube(float length, float width, float height, float stepSize)
{
    std::vector<glm::ivec3> discreteCoord;
    std::vector<Aabb> aabbs;

    int widthSize = std::ceil(width / stepSize);
    int heightSize = std::ceil(height / stepSize);
    int lengthSize = std::ceil(length / stepSize);

    float widthCenter = widthSize / 2.0;
    float heightCenter = heightSize / 2.0;
    float lengthCenter = lengthSize/ 2.0;

    // i,j为层，k为列
    for (int k = 0; k < heightSize; k++)
    {
    for (int i = 0; i < lengthSize; i++)
    {
        for (int j = 0; j < widthSize; j++)
        {
            
                glm::vec3 coord = {i, j, k};
                Aabb aabb = {{coord},
                             {coord + glm::vec3(1)}};
                discreteCoord.emplace_back(coord);
                aabbs.emplace_back(aabb);
            }
        }
    }

    m_voxelModel.centerPoints = discreteCoord;
    m_voxelModel.aabbs = aabbs;
    return discreteCoord;
}

PrimMesh VoxelDesigner::createTriVoxels(const std::vector<glm::ivec3>& voxelIds)
{
    PrimMesh mesh{};
    mesh.meshId = 0;

    auto appendQuad = [&mesh](const glm::vec3& p0, const glm::vec3& p1,
                              const glm::vec3& p2, const glm::vec3& p3) {
        const glm::vec3 points[4] = {p0, p1, p2, p3};
        const uint32_t order[6] = {0, 1, 2, 0, 2, 3};
        for (uint32_t index : order) {
            VertexAttribute vertex{};
            vertex.pos = points[index];
            mesh.indices.emplace_back(static_cast<uint32_t>(mesh.vertices.size()));
            mesh.vertices.emplace_back(vertex);
        }
    };

    glm::vec3 centerSum(0.0f);
    for (const glm::ivec3& id : voxelIds) {
        const glm::vec3 base(id);
        const float x = base.x;
        const float y = base.y;
        const float z = base.z;

        appendQuad({x, y, z + 0.5f}, {x + 1.0f, y, z + 0.5f},
                   {x + 1.0f, y + 1.0f, z + 0.5f}, {x, y + 1.0f, z + 0.5f});
        appendQuad({x + 0.5f, y, z}, {x + 0.5f, y + 1.0f, z},
                   {x + 0.5f, y + 1.0f, z + 1.0f}, {x + 0.5f, y, z + 1.0f});
        appendQuad({x, y + 0.5f, z}, {x, y + 0.5f, z + 1.0f},
                   {x + 1.0f, y + 0.5f, z + 1.0f}, {x + 1.0f, y + 0.5f, z});

        mesh.voxelIds.emplace_back(id);
        mesh.isValids.emplace_back(int5{1, 1, 1, 1, 1});
        mesh.faceIds.emplace_back(0);
        centerSum += base + glm::vec3(0.5f);
    }

    mesh.nVertices = static_cast<uint32_t>(mesh.vertices.size());
    mesh.nIndices = static_cast<uint32_t>(mesh.indices.size());
    mesh.meshcenter = voxelIds.empty() ? glm::vec3(0.0f)
                                      : centerSum / static_cast<float>(voxelIds.size());
    return mesh;
}

PrimMesh VoxelDesigner::createTriCube(Shape shape, float stepSize)
{
    PrimMesh voxelTriModel;
    voxelTriModel.nVertices = 0;
    voxelTriModel.nIndices = 0;
    voxelTriModel.meshId = 0;

    // 绘制每一层x,y平面
    for (int k = 0; k < shape.height / stepSize; k++) //height,绘制每一层的面
    {
        for (int i = 0; i < shape.length / stepSize; i++)
        {
            for (int j = 0; j < shape.width / stepSize; j++)
            {
                glm::vec3 points[4];
                points[0].x = i;
                points[0].y = j;
                points[0].z = k;

                points[1].x = (i);
                points[1].y = (j + 1);
                points[1].z = k;

                points[2].x = (i + 1);
                points[2].y = (j + 1);
                points[2].z = k;

                points[3].x = (i + 1);
                points[3].y = (j);
                points[3].z = k;

                 int list[] = {0, 1, 2, 0, 2, 3};
                for (int kk = 0; kk < 6; kk++)
                {
                    int listK = list[kk];
                    VertexAttribute va{points[listK]};
                    voxelTriModel.vertices.emplace_back(va);

                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kk);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }
        }
    }
    
    // 绘制每一层y,z平面
    for (int k = 0; k < shape.height / stepSize; k++) //height,绘制每一层的面
    {
        for (int i = 0; i < shape.length / stepSize; i++)
        {
            for (int j = 0; j < shape.width / stepSize; j++)
            {
                glm::vec3 points[4];
                points[0].x = i;
                points[0].y = j;
                points[0].z = k;

                points[1].x = i;
                points[1].y = (j + 1);
                points[1].z = k;

                points[2].x = i;
                points[2].y = (j + 1);
                points[2].z = (k + 1);

                points[3].x = i;
                points[3].y = j;
                points[3].z = (k + 1);

                 int list[] = {0, 1, 2, 0, 2, 3};
                for (int kk = 0; kk < 6; kk++)
                {
                    int listK = list[kk];
                    VertexAttribute va{points[listK]};
                    voxelTriModel.vertices.emplace_back(va);

                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kk);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }
        }
    }
   
    // 绘制每一层x,z平面
    for (int k = 0; k < shape.height / stepSize; k++) //height,绘制每一层的面
    {
        for (int i = 0; i < shape.length / stepSize; i++)
        {
            for (int j = 0; j < shape.width / stepSize; j++)
            {
                glm::vec3 points[4];
                points[0].x = i;
                points[0].y = j;
                points[0].z = k;

                points[1].x = (i + 1);
                points[1].y = j;
                points[1].z = k;

                points[2].x = (i + 1);
                points[2].y = j;
                points[2].z = (k + 1);

                points[3].x = i;
                points[3].y = j ;
                points[3].z = (k + 1);

                int list[] = {0, 1, 2, 0, 2, 3};
                for (int kk = 0; kk < 6; kk++)
                {
                    int listK = list[kk];
                    VertexAttribute va{points[listK]};
                    voxelTriModel.vertices.emplace_back(va);

                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kk);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
                voxelTriModel.voxelIds.emplace_back(glm::vec3{i, j, k});
            }
        }
    }

    // 绘制顶面(x,y)
    for (int i = 0; i < shape.length / stepSize; i++)
    {
        for (int j = 0; j < shape.width / stepSize; j++)
        {
            glm::vec3 points[4];
            points[0].x = i;
            points[0].y = j ;
            points[0].z = round(shape.height /stepSize);

            points[1].x = (i);
            points[1].y = (j + 1);
            points[1].z = round(shape.height/stepSize);

            points[2].x = (i + 1);
            points[2].y = (j + 1);
            points[2].z = round(shape.height/stepSize);

            points[3].x = (i + 1);
            points[3].y = (j);
            points[3].z = round(shape.height/stepSize);

            int list[] = {0, 1, 2, 0, 2, 3};
            for (int kk = 0; kk < 6; kk++)
            {
                int listK = list[kk];
                VertexAttribute va{points[listK]};
                voxelTriModel.vertices.emplace_back(va);

                voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kk);
            }

            voxelTriModel.nVertices += 6;
            voxelTriModel.nIndices += 6;
        }
    }

    // 绘制y,z最大面
    for (int k = 0; k < shape.height / stepSize; k++) //height,绘制每一层的面
    {
            for (int j = 0; j < shape.width / stepSize; j++)
            {
                glm::vec3 points[4];
                points[0].x = round(shape.length/stepSize);
                points[0].y = j;
                points[0].z = k;

                points[1].x = round(shape.length/stepSize);
                points[1].y = (j + 1);
                points[1].z = k;

                points[2].x = round(shape.length/stepSize);
                points[2].y = (j + 1);
                points[2].z = (k + 1);

                points[3].x = round(shape.length/stepSize);
                points[3].y = j;
                points[3].z = (k + 1);

                int list[] = {0, 1, 2, 0, 2, 3};
                for (int kk = 0; kk < 6; kk++)
                {
                    int listK = list[kk];
                    VertexAttribute va{points[listK]};
                    voxelTriModel.vertices.emplace_back(va);

                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kk);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }
    }

    // 绘制x,z最大面
    for (int k = 0; k < shape.height / stepSize; k++) //height,绘制每一层的面
    {
        for (int i = 0; i < shape.length / stepSize; i++)
        {
                glm::vec3 points[4];
                points[0].x = i;
                points[0].y = round(shape.width/stepSize);
                points[0].z = k;

                points[1].x = (i + 1);
                points[1].y = round(shape.width/stepSize);
                points[1].z = k;

                points[2].x = (i + 1);
                points[2].y = round(shape.width/stepSize);
                points[2].z = (k + 1);

                points[3].x = i;
                points[3].y = round(shape.width/stepSize);
                points[3].z = (k + 1);

                int list[] = {0, 1, 2, 0, 2, 3};
                for (int kk = 0; kk < 6; kk++)
                {
                    int listK = list[kk];
                    VertexAttribute va{points[listK]};
                    voxelTriModel.vertices.emplace_back(va);

                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kk);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
        }
    }


    // 计算模型的x,y平面坐标下中心点位置
   voxelTriModel.meshcenter = glm::vec3(shape.length / 2.0, shape.width / 2.0, 0 / 2.0);

    return voxelTriModel;
}

PrimMesh VoxelDesigner::createTriCube_wall(Shape shape, float stepSize)
{
    PrimMesh voxelTriModel;
    voxelTriModel.nVertices = 0;
    voxelTriModel.nIndices = 0;
    voxelTriModel.meshId = 0;

    // 绘制每一层x,y平面
    for (int k = 0; k < shape.height / stepSize-1; k++) //height,绘制每一层的面
    {
        for (int i = 0; i < shape.length / stepSize; i++)
        {
            for (int j = 0; j < shape.width / stepSize; j++)
            {
                glm::vec3 points[4];
                points[0].x = i;
                points[0].y = j;
                points[0].z = k;

                points[1].x = (i);
                points[1].y = (j + 1);
                points[1].z = k;

                points[2].x = (i + 1);
                points[2].y = (j + 1);
                points[2].z = k;

                points[3].x = (i + 1);
                points[3].y = (j);
                points[3].z = k;

                int list[] = {0, 1, 2, 0, 2, 3};
                for (int kk = 0; kk < 6; kk++)
                {
                    int listK = list[kk];
                    VertexAttribute va{points[listK]};
                    voxelTriModel.vertices.emplace_back(va);

                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kk);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }
        }
    }

    // 绘制每一层y,z平面
    for (int k = 0; k < shape.height / stepSize-1; k++) //height,绘制每一层的面
    {
        for (int i = 0; i < shape.length / stepSize; i++)
        {
            for (int j = 0; j < shape.width / stepSize; j++)
            {
                glm::vec3 points[4];
                points[0].x = i;
                points[0].y = j;
                points[0].z = k;

                points[1].x = i;
                points[1].y = (j + 1);
                points[1].z = k;

                points[2].x = i;
                points[2].y = (j + 1);
                points[2].z = (k + 1);

                points[3].x = i;
                points[3].y = j;
                points[3].z = (k + 1);

                int list[] = {0, 1, 2, 0, 2, 3};
                for (int kk = 0; kk < 6; kk++)
                {
                    int listK = list[kk];
                    VertexAttribute va{points[listK]};
                    voxelTriModel.vertices.emplace_back(va);

                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kk);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }
        }
    }

    // 绘制每一层x,z平面
    for (int k = 0; k < shape.height / stepSize-1; k++) //height,绘制每一层的面
    {
        for (int i = 0; i < shape.length / stepSize; i++)
        {
            for (int j = 0; j < shape.width / stepSize; j++)
            {
                glm::vec3 points[4];
                points[0].x = i;
                points[0].y = j;
                points[0].z = k;

                points[1].x = (i + 1);
                points[1].y = j;
                points[1].z = k;

                points[2].x = (i + 1);
                points[2].y = j;
                points[2].z = (k + 1);

                points[3].x = i;
                points[3].y = j ;
                points[3].z = (k + 1);

                int list[] = {0, 1, 2, 0, 2, 3};
                for (int kk = 0; kk < 6; kk++)
                {
                    int listK = list[kk];
                    VertexAttribute va{points[listK]};
                    voxelTriModel.vertices.emplace_back(va);

                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kk);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
                voxelTriModel.voxelIds.emplace_back(glm::vec3{i, j, k});
                voxelTriModel.isValids.emplace_back(int5{1, 1, 1, 1, 1});
                voxelTriModel.faceIds.emplace_back(0);
            }
        }
    }


    // 绘制y,z最大面
    for (int k = 0; k < shape.height / stepSize-1; k++) //height,绘制每一层的面
    {
        for (int j = 0; j < shape.width / stepSize; j++)
        {
            glm::vec3 points[4];
            points[0].x = shape.length/stepSize;
            points[0].y = j;
            points[0].z = k;

            points[1].x = shape.length/ stepSize;
            points[1].y = (j + 1);
            points[1].z = k;

            points[2].x = shape.length/ stepSize;
            points[2].y = (j + 1);
            points[2].z = (k + 1);

            points[3].x = shape.length/ stepSize;
            points[3].y = j;
            points[3].z = (k + 1);

            int list[] = {0, 1, 2, 0, 2, 3};
            for (int kk = 0; kk < 6; kk++)
            {
                int listK = list[kk];
                VertexAttribute va{points[listK]};
                voxelTriModel.vertices.emplace_back(va);

                voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kk);
            }

            voxelTriModel.nVertices += 6;
            voxelTriModel.nIndices += 6;
        }
    }

    // 绘制x,z最大面
    for (int k = 0; k < shape.height / stepSize-1; k++) //height,绘制每一层的面
    {
        for (int i = 0; i < shape.length / stepSize; i++)
        {
            glm::vec3 points[4];
            points[0].x = i;
            points[0].y = shape.width/ stepSize;
            points[0].z = k;

            points[1].x = (i + 1);
            points[1].y = shape.width/ stepSize;
            points[1].z = k;

            points[2].x = (i + 1);
            points[2].y = shape.width/ stepSize;
            points[2].z = (k + 1);

            points[3].x = i;
            points[3].y = shape.width/ stepSize;
            points[3].z = (k + 1);

            int list[] = {0, 1, 2, 0, 2, 3};
            for (int kk = 0; kk < 6; kk++)
            {
                int listK = list[kk];
                VertexAttribute va{points[listK]};
                voxelTriModel.vertices.emplace_back(va);

                voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kk);
            }

            voxelTriModel.nVertices += 6;
            voxelTriModel.nIndices += 6;
        }
    }


    // 计算模型的x,y平面坐标下中心点位置
    voxelTriModel.meshcenter = glm::vec3(shape.length / 2.0, shape.width / 2.0, 0 / 2.0);

    return voxelTriModel;
}

PrimMesh VoxelDesigner::createTriCube_roof(Shape shape, float stepSize)
{
    PrimMesh voxelTriModel;
    voxelTriModel.nVertices = 0;
    voxelTriModel.nIndices = 0;
    voxelTriModel.meshId = 0;


    // 绘制顶面(x,y)
    int k = shape.height/stepSize - 1;
    for (int i = 0; i < shape.length / stepSize; i++)
    {
        for (int j = 0; j < shape.width / stepSize; j++)
        {
            glm::vec3 points[4];
            points[0].x = i;
            points[0].y = j ;
            points[0].z = k+1;

            points[1].x = (i);
            points[1].y = (j + 1);
            points[1].z = k+1;

            points[2].x = (i + 1);
            points[2].y = (j + 1);
            points[2].z = k+1;

            points[3].x = (i + 1);
            points[3].y = (j);
            points[3].z = k+1;

            int list[] = {0, 1, 2, 0, 2, 3};
            for (int kk = 0; kk < 6; kk++)
            {
                int listK = list[kk];
                VertexAttribute va{points[listK]};
                voxelTriModel.vertices.emplace_back(va);

                voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kk);
            }

            voxelTriModel.nVertices += 6;
            voxelTriModel.nIndices += 6;
            voxelTriModel.voxelIds.emplace_back(glm::vec3{i, j, k});
            voxelTriModel.isValids.emplace_back(int5{1, 1, 1, 1, 1});
            voxelTriModel.faceIds.emplace_back(0);
        }
    }

    // 计算模型的x,y平面坐标下中心点位置
    voxelTriModel.meshcenter = glm::vec3(shape.length / 2.0, shape.width / 2.0, 0 / 2.0);

    return voxelTriModel;
}

bool VoxelDesigner::outputPrimMesh(std::string path, PrimMesh mesh)
{
    std::ofstream outfile(path);
    if (outfile.is_open())
    {
        for(int kk=0;kk< mesh.vertices.size();kk++)
            outfile << "v "<<mesh.vertices[kk].pos.x <<  " "
                    <<mesh.vertices[kk].pos.z << " "
                    <<mesh.vertices[kk].pos.y << " "<<std::endl;

        uint32_t m_indicesBase = 0;
        for (int kk = 0; kk < mesh.indices.size(); kk=kk+3)
            outfile << "f " << mesh.indices[kk]+1+ m_indicesBase << " "
                    << mesh.indices[kk+1]+1+ m_indicesBase << " "
                    << mesh.indices[kk+2]+1+ m_indicesBase << " " << std::endl;
        m_indicesBase += mesh.nVertices;
        outfile.close();
        return true;
    }
    return false;
}


PrimMesh VoxelDesigner::createTriEllipsoid(Shape shape, float stepSize)
{
    PrimMesh voxelTriModel;

    voxelTriModel.nVertices = 0;
    voxelTriModel.nIndices = 0;
    voxelTriModel.meshId = 0;

    int widthSize = std::ceil(shape.width / stepSize);
    int heightSize = std::ceil(shape.height / stepSize);
    int lengthSize = std::ceil(shape.length / stepSize);

    float widthCenter = (shape.width / stepSize) / 2.0;
    float heightCenter = (shape.height / stepSize) / 2.0;
    float lengthCenter = (shape.length / stepSize) / 2.0;

    for (int i = 0; i < lengthSize; i++)
    {
        for (int j = 0; j < widthSize; j++)
        {
            for (int k = 0; k < heightSize; k++)
            {
                float dist = (i - lengthCenter + 0.5) * (i - lengthCenter + 0.5) / (lengthCenter * lengthCenter) +
                             (j - widthCenter + 0.5) * (j - widthCenter + 0.5) / (widthCenter * widthCenter) +
                             (k - heightCenter + 0.5) * (k - heightCenter + 0.5) / (heightCenter * heightCenter);
                // left, forward and bottom face
                if (dist <= 1.0)
                {
                    glm::ivec3 voxel = glm::ivec3(i,j,k);
                    glm::vec3 point[8];

                    point[0].x = voxel.x ;
                    point[0].y = voxel.y ;
                    point[0].z = voxel.z ;

                    point[1].x = voxel.x ;
                    point[1].y = (voxel.y + 1) ;
                    point[1].z = voxel.z ;

                    point[2].x = (voxel.x + 1) ;
                    point[2].y = (voxel.y + 1) ;
                    point[2].z = voxel.z ;

                    point[3].x = (voxel.x + 1) ;
                    point[3].y = voxel.y ;
                    point[3].z = voxel.z ;

                    point[4].x = voxel.x ;
                    point[4].y = voxel.y ;
                    point[4].z = (voxel.z + 1) ;

                    point[5].x = voxel.x ;
                    point[5].y = (voxel.y + 1) ;
                    point[5].z = (voxel.z + 1) ;

                    point[6].x = voxel.x +1;
                    point[6].y = (voxel.y + 1) ;
                    point[6].z = (voxel.z + 1) ;

                    point[7].x = (voxel.x + 1) ;
                    point[7].y = voxel.y;
                    point[7].z = (voxel.z + 1) ;

                    for (int vi = 0; vi < 6; vi++)
                    {
                        VertexAttribute va;
                        va.pos = point[vi];
                        voxelTriModel.vertices.emplace_back(va);
                    }

                    // 0,1,2,3 bottom
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 1);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 2);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 2);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 3);

                    // 0,1,4,5 left
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 1);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 5);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 4);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 5);

                    // 0,3,4,6 forward
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 4);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 6);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 3);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 6);

                    voxelTriModel.nVertices += 6;
                    voxelTriModel.nIndices += 6 * 3;

                   voxelTriModel.voxelIds.emplace_back(glm::vec3{i, j, k});
                }

                // up face
                float upDist = (i - lengthCenter + 0.5) * (i - lengthCenter + 0.5) / (lengthCenter * lengthCenter) +
                               (j - widthCenter + 0.5) * (j - widthCenter + 0.5) / (widthCenter * widthCenter) +
                               (k - heightCenter + 1.5) * (k - heightCenter + 1.5) / (heightCenter * heightCenter);
                if (dist <= 1.0 && upDist > 1.0)
                {
                    glm::ivec3 voxel = glm::ivec3(i, j, k);
                    glm::vec3 point[4];

                    point[0].x = voxel.x ;
                    point[0].y = voxel.y ;
                    point[0].z = (voxel.z + 1) ;

                    point[1].x = voxel.x ;
                    point[1].y = (voxel.y + 1) ;
                    point[1].z = (voxel.z + 1) ;

                    point[2].x = (voxel.x + 1) ;
                    point[2].y = (voxel.y + 1) ;
                    point[2].z = (voxel.z + 1) ;

                    point[3].x = (voxel.x + 1) ;
                    point[3].y = voxel.y ;
                    point[3].z = (voxel.z + 1) ;

                    for (int vi = 0; vi < 4; vi++)
                    {
                        VertexAttribute va;
                        va.pos = point[vi];
                        voxelTriModel.vertices.emplace_back(va);
                    }

                    // 0,1,2,3 up
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 1);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 2);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 2);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 3);

                    voxelTriModel.nVertices += 4;
                    voxelTriModel.nIndices += 2 * 3;
                }

                // backward face
                float backwardDist = (i - lengthCenter + 0.5) * (i - lengthCenter + 0.5) / (lengthCenter * lengthCenter) +
                                     (j - widthCenter + 1.5) * (j - widthCenter + 1.5) / (widthCenter * widthCenter) +
                                     (k - heightCenter + 0.5) * (k - heightCenter + 0.5) / (heightCenter * heightCenter);
                if (dist <= 1.0 && backwardDist > 1.0)
                {
                    glm::ivec3 voxel = glm::ivec3(i, j, k);
                    glm::vec3 point[4];

                    point[0].x = voxel.x ;
                    point[0].y = (voxel.y + 1) ;
                    point[0].z = voxel.z ;

                    point[1].x = voxel.x ;
                    point[1].y = (voxel.y + 1) ;
                    point[1].z = (voxel.z + 1) ;

                    point[2].x = (voxel.x + 1) ;
                    point[2].y = (voxel.y + 1) ;
                    point[2].z = (voxel.z + 1) ;

                    point[3].x = (voxel.x + 1) ;
                    point[3].y = (voxel.y + 1) ;
                    point[3].z = voxel.z ;

                    for (int vi = 0; vi < 4; vi++)
                    {
                        VertexAttribute va;
                        va.pos = point[vi];
                        voxelTriModel.vertices.emplace_back(va);
                    }

                    // 0,1,2,3 up
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 1);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 2);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 2);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 3);

                    voxelTriModel.nVertices += 4;
                    voxelTriModel.nIndices += 2 * 3;
                }

                // right face
                float rightDist = (i - lengthCenter + 1.5) * (i - lengthCenter + 1.5) / (lengthCenter * lengthCenter) +
                                  (j - widthCenter + 0.5) * (j - widthCenter + 0.5) / (widthCenter * widthCenter) +
                                  (k - heightCenter + 0.5) * (k - heightCenter + 0.5) / (heightCenter * heightCenter);
                if (dist <= 1.0 && rightDist > 1.0)
                {
                    glm::ivec3 voxel = glm::ivec3(i, j, k);
                    glm::vec3 point[4];

                    point[0].x = (voxel.x + 1) ;
                    point[0].y = voxel.y ;
                    point[0].z = voxel.z ;

                    point[1].x = (voxel.x + 1) ;
                    point[1].y = (voxel.y + 1) ;
                    point[1].z = voxel.z ;

                    point[2].x = (voxel.x + 1) ;
                    point[2].y = (voxel.y + 1) ;
                    point[2].z = (voxel.z + 1) ;

                    point[3].x = (voxel.x + 1) ;
                    point[3].y = voxel.y ;
                    point[3].z = (voxel.z + 1) ;

                    for (int vi = 0; vi < 4; vi++)
                    {
                        VertexAttribute va;
                        va.pos = point[vi];
                        voxelTriModel.vertices.emplace_back(va);
                    }

                    // 0,1,2,3 up
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 1);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 2);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 2);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + 3);

                    voxelTriModel.nVertices += 4;
                    voxelTriModel.nIndices += 2 * 3;
                }
            }
        }
    }


    voxelTriModel.meshcenter = glm::vec3(shape.length / 2.0, shape.width / 2.0, shape.height / 2.0);

    return voxelTriModel;
}

bool VoxelDesigner::isInPloy(nvmath::vec2i testPoint, std::vector<nvmath::vec2f> ploys)
{
    nvmath::vec2f A = ploys[0];
    nvmath::vec2f B = ploys[1];
    nvmath::vec2f C = ploys[2];
    nvmath::vec2f D = ploys[3];
    int y = testPoint[1];
    int x = testPoint[0];
    int a = (B.x - A.x) * (y - A.y) - (B.y - A.y) * (x - A.x);
    int b = (C.x - B.x) * (y - B.y) - (C.y - B.y) * (x - B.x);
    int c = (D.x - C.x) * (y - C.y) - (D.y - C.y) * (x - C.x);
    int d = (A.x - D.x) * (y - D.y) - (A.y - D.y) * (x - D.x);
    if ((a > 0 && b > 0 && c > 0 && d > 0) || (a < 0 && b < 0 && c < 0 && d < 0))
    {
        return true;
    }

    return false;
}



std::vector<glm::ivec3> VoxelDesigner::createBackground(float sceneLength, float sceneWidth, float stepSize)
{
    std::vector<glm::ivec3> discreteCoord;
    std::vector<Aabb> aabbs;
    int lengthSize = sceneLength / stepSize;
    int widthSize = sceneWidth /stepSize;

    int a = 0;
    for (int j = 0; j < lengthSize; j++)
    {
        for (int k = 0; k < widthSize; k++)
        {
            glm::ivec3 coord = glm::ivec3(j, k, 0);
            Aabb aabb = {{glm::vec3(coord) },
                {glm::vec3(coord) + glm::vec3(1.0)}};
            discreteCoord.emplace_back(coord);
            aabbs.emplace_back(aabb);
        }
    }

//    m_voxelModel.centerPoints = discreteCoord;
//    m_voxelModel.aabbs = aabbs;
    return discreteCoord;
}

/// Create a triangle background
/// \param sceneLength
/// \param sceneWidth
/// \param stepSize
/// create mesh on upper face, voxelId is left-down point
/// \return a mesh with XYZ coordinate;
PrimMesh VoxelDesigner::createTriBackground(float sceneLength, float sceneWidth, float stepSize)
{
    PrimMesh mesh;
    mesh.meshId = 0;
    mesh.nVertices = 0;
    mesh.nIndices = 0;
    float bgZ = -1;
    // float bgZ = 0;


    for (int i = 0; i < sceneLength/stepSize; i++)
    {
        for (int j = 0; j < sceneWidth/stepSize; j++)
        {
            glm::vec3 points[4];
            points[0].x = i ;
            points[0].y = j ;
            points[0].z = bgZ +1;

            points[1].x = (i) ;
            points[1].y = (j + 1) ;
            points[1].z = bgZ +1;

            points[2].x = (i + 1) ;
            points[2].y = (j + 1) ;
            points[2].z = bgZ +1;

            points[3].x = (i + 1) ;
            points[3].y = (j) ;
            points[3].z = bgZ +1;

            int list[] = {0, 1, 2, 0, 2, 3};
            for (int k = 0;k < 6; k++)
            {
                int listK = list[k];
                VertexAttribute va{points[listK]};
                mesh.vertices.emplace_back(va);
                mesh.indices.emplace_back(mesh.nVertices + k);
            }

            mesh.nVertices += 6;
            mesh.nIndices += 6;
            mesh.voxelIds.emplace_back(glm::vec3{i , j , bgZ });
        }
    }

    return mesh;
}

PrimMesh VoxelDesigner::createTriEntity(Shape shape, float stepSize)
{
    PrimMesh voxelTriModel;
    ShapeType shapeType = shape.shapetype;

    if (shapeType == ShapeType::CUBE)
    {
        voxelTriModel = createTriCube(shape, stepSize);
    }
    if (shapeType == ShapeType::ELLIPSOID)
    {
        voxelTriModel = createTriEllipsoid(shape, stepSize);
    }

    return voxelTriModel;
}

//VoxelTriModel VoxelDesigner::createTriEntity_rotate(ShapeType shapeType, Shape shape, float stepSize, float rotateAngle)
//{
//    VoxelTriModel voxelTriModel;
//    if (shapeType == ShapeType::CUBE)
//    {
//        voxelTriModel = createTriCube_rotate(shape, stepSize, rotateAngle);
//    }
//    if (shapeType == ShapeType::ELLIPSOID)
//    {
//        voxelTriModel = createTriEllipsoid(shape, stepSize);
//    }
//    return voxelTriModel;
//}

//VoxelTriModel VoxelDesigner::createTriBackgroundFromDEM(const std::string &filename, glm::vec3 sceneSize_XYZ, _2D::BilinearInterpolator<double> &interp)
//{
//    VoxelTriModel voxelTriModel;
//    std::string infile = filename;
//    GDALAllRegister();
//
//    BYTE *pafScanblock;
//    GDALDataset *poDataset;
//    poDataset = (GDALDataset *)GDALOpen(infile.c_str(), GA_ReadOnly);
//    if (poDataset == NULL)
//    {
//        std::cout << "fail in open files!" << std::endl;
//        return voxelTriModel;
//    }
//    int nImgSizeX = poDataset->GetRasterXSize();
//    int nImgSizeY = poDataset->GetRasterYSize();
//    int bandcount = poDataset->GetRasterCount();
//
//    int num_image_size = 0;
//
//    FLOAT *pafScanline = new FLOAT[nImgSizeX * nImgSizeY];
//    poDataset->RasterIO(GF_Read, 0, 0, nImgSizeX, nImgSizeY, pafScanline, nImgSizeX, nImgSizeY, GDT_Float32, bandcount, 0, 0, 0, 0);
//
//    m_scale = sceneSize_XYZ.x / nImgSizeX;
//    float scale = m_scale;
//
//    std::vector<double> x, y, z;
//    for (int i = 0; i < nImgSizeX; i++)
//    {
//        for (int j = 0; j < nImgSizeY; j++)
//        {
//            x.emplace_back((i * scale + scale / 2.0)); // pixel center location
//            y.emplace_back((j * scale + scale / 2.0));
//            z.emplace_back(pafScanline[i * nImgSizeY + j]);
//        }
//    }
//
//
//    interp.setData(x, y, z);
//
//    delete[] pafScanline;
//    delete poDataset;
//
//    float step = 1; // / 2.0; // control DEM subdivision degree, The smaller the value, the finer the distinction
//
//    //找寻voxel中心点,后加上中心点包含的体
//    uint32_t nVertices = 0;
//    uint32_t nIndices = 0;
//    minElevation = z[0];
//    for(int j=0; j < sceneSize_XYZ.x; j=j+step)
//    {
//        for (int k=0; k<sceneSize_XYZ.y; k=k+step)
//        {
//            float jj = j, kk= k;
//            if (jj < scale / 2)
//                jj = scale / 2.0;
//            if (kk < scale / 2)
//                kk = scale / 2.0;
//            if (jj > sceneSize_XYZ.x - scale / 2.0)
//                jj = sceneSize_XYZ.x - scale / 2.0 - 0.01;
//            if (kk > sceneSize_XYZ.y - scale / 2.0)
//                kk = sceneSize_XYZ.y - scale / 2.0 - 0.01;
//            voxelTriModel.centerPoints.emplace_back(glm::vec3{j + 0.5, k + 0.5,
//                                                                  int(interp(jj, kk))});
//
//            
//            glm::ivec3 voxel = glm::ivec3(j, k, int(interp(jj, kk)));
//
//            if (voxel.z < minElevation)
//                minElevation = voxel.z;
//            glm::vec3 point[8];
//            int stepSize = 1;
//            point[0].x = voxel.x * stepSize;
//            point[0].y = voxel.y * stepSize;
//            point[0].z = voxel.z * stepSize;
//
//            point[1].x = voxel.x * stepSize;
//            point[1].y = (voxel.y + 1) * stepSize;
//            point[1].z = voxel.z * stepSize;
//
//            point[2].x = (voxel.x + 1) * stepSize;
//            point[2].y = (voxel.y + 1) * stepSize;
//            point[2].z = voxel.z * stepSize;
//
//            point[3].x = (voxel.x + 1) * stepSize;
//            point[3].y = voxel.y * stepSize;
//            point[3].z = voxel.z * stepSize;
//
//            point[4].x = voxel.x * stepSize;
//            point[4].y = voxel.y * stepSize;
//            point[4].z = (voxel.z + 1) * stepSize;
//
//            point[5].x = voxel.x * stepSize;
//            point[5].y = (voxel.y + 1) * stepSize;
//            point[5].z = (voxel.z + 1) * stepSize;
//
//            point[6].x = (voxel.x + 1) * stepSize;
//            point[6].y = voxel.y * stepSize;
//            point[6].z = (voxel.z + 1) * stepSize;
//
//            point[7].x = (voxel.x + 1) * stepSize;
//            point[7].y = (voxel.y + 1) * stepSize;
//            point[7].z = (voxel.z + 1) * stepSize;
//
//            int list[] = {0, 1, 2, 0, 2, 3,
//                          0, 1, 5, 0, 4, 5,
//                          1, 2, 6, 1, 5, 6,
//                          4, 5, 6, 4, 6, 7,
//                          2, 3, 6, 3, 6, 7,
//                          0, 3, 7, 0, 4, 7};
//
//
//            //顶点
//            for (int kkk = 0; kkk < 36; kkk++)
//            {
//                int listK = list[kkk];
//                VertexAttribute va{point[listK]};
//                voxelTriModel.vertices.emplace_back(va);
//
//                voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + kkk);
//
//
//                //VertexAttribute va{point[kkk]};
//                //voxelTriModel.vertices.emplace_back(va);
//            }
//            // 边
//            /*for (int kkk = 0; kkk < 36; kkk++)
//            {
//                int listK = list[kkk];
//
//                voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + listK);
//            }*/
//            voxelTriModel.nVertices += 36;
//            voxelTriModel.nIndices += 36;
//        }
//    }
//
//    return voxelTriModel;
//}

PrimMesh VoxelDesigner::createTriEntitiesFromTif(std::string heightPath, float stepSize) {
    PrimMesh voxelTriModel;
    voxelTriModel.nVertices=0;
    voxelTriModel.nIndices = 0;
    voxelTriModel.meshId = 0;
    //===================================================================
    /*! height map
     */
    CPLSetConfigOption("gdal_filename_is_utf8", "no");
    GDALAllRegister();

    GDALDataset* heightDataset;
    heightDataset = (GDALDataset*)GDALOpen(heightPath.c_str(), GA_ReadOnly);
    if (heightDataset == NULL)
    {
        std::cout << "fail in open height files!" << std::endl;
        return voxelTriModel;
    }
    int nImgSizeX = heightDataset->GetRasterXSize();
    int nImgSizeY = heightDataset->GetRasterYSize();
    int bandcount = heightDataset->GetRasterCount();
    FLOAT* heightScanline = new FLOAT[nImgSizeX * nImgSizeY];
    heightDataset->RasterIO(GF_Read, 0, 0, nImgSizeX, nImgSizeY, heightScanline, nImgSizeX, nImgSizeY, GDT_Float32, bandcount, 0, 0, 0, 0);


    //===================================================================
    /*! create every model through its height: get the center points
     * (this center points is actually the minimum(left and down) point)
     * for edge pixel: create the vertical voxels from the lowest height around to its height.
     *              if the height of this pixel is the lowest, then establish its corresponding voxel height.
     * for non-edge pixel: establish its corresponding voxel height.
     * ENVI tif coordinate system:
     *      .--------> x
     *      |---------
     *      |----.(i, j)  (j * nImgX + i)
     *      |
     *      y
     *
     * spatial relationship:
     *      .------------------------------------------------> x
     *      |----------------- .up (back)----------------------
     *      |--- .left (left)| .cur (center) | .right (right)----
     *      |----------------- .down (forward) ----------------
     *      |-------------------------------------------------
     *      y
     */

    FLOAT* minReHeightScanline = new FLOAT[nImgSizeX * nImgSizeY];

    float minReHeight, rightReHeight, leftReHeight, upReHeight, downReHeight;
    for (int i = 1; i < nImgSizeX - 1; i++)
    {
        for (int j = 1; j < nImgSizeY - 1; j++)
        {
            // all the height
            float curReHeight = std::round(heightScanline[j * nImgSizeX + i] / stepSize);


            leftReHeight = std::round(heightScanline[j * nImgSizeX + i - 1] / stepSize);
            rightReHeight = std::round(heightScanline[j * nImgSizeX + i + 1] / stepSize);
            upReHeight = std::round(heightScanline[(j - 1) * nImgSizeX + i] / stepSize);
            downReHeight = std::round(heightScanline[(j + 1) * nImgSizeX + i] / stepSize);
            // resized height
            minReHeight = std::min({ curReHeight, leftReHeight, rightReHeight, upReHeight, downReHeight });

            int faceId = 0;
            if (downReHeight == minReHeight) faceId = 1;    // forward
            if (upReHeight == minReHeight) faceId = 4;      // backward
            if (leftReHeight == minReHeight) faceId = 3;    // left
            if (rightReHeight == minReHeight) faceId = 2;   // right
            if (curReHeight == minReHeight) faceId = 5;     // up

            minReHeightScanline[j * nImgSizeX + i] = minReHeight;
            //===================================================================
            /*! save [voxelIds: left-down point] and [faceIds] values: NanoVDB attribution location
             * {}
             */
            if (curReHeight == minReHeight)
            {
                if (curReHeight == 0) continue;

                voxelTriModel.voxelIds.emplace_back(i, j, curReHeight - 1);
                voxelTriModel.faceIds.emplace_back(faceId);

                // isValids: 0 forward, 1 backward, 2 left, 3 right, 4 up
                int5 curIsValids = { 0, 0, 0, 0, 1 };
                voxelTriModel.isValids.emplace_back(curIsValids);
            }
            else
            {
                for (int heighti = 0; heighti < (curReHeight - minReHeight); heighti++)
                {
                    voxelTriModel.voxelIds.emplace_back(i, j, minReHeight + heighti);
                    voxelTriModel.faceIds.emplace_back(faceId);

                    // isValids: 0 forward, 1 backward, 2 left, 3 right, 4 up
                    int5 curIsValids = { 0, 0, 0, 0, 0 };
                    if (minReHeight + heighti > downReHeight) curIsValids.values[0] = 1;
                    if (minReHeight + heighti > upReHeight) curIsValids.values[1] = 1;
                    if (minReHeight + heighti > leftReHeight) curIsValids.values[2] = 1;
                    if (minReHeight + heighti > rightReHeight) curIsValids.values[3] = 1;
                    voxelTriModel.isValids.emplace_back(curIsValids);
                }
            }

            //===================================================================
            /*! save [vertices] and [indices] values: mesh
             * because add voxel wastes the memory, so add each face
             */
            // because add voxel wastes the memory, so add each face
            if (curReHeight == minReHeight)    // add up face
            {
                if (curReHeight == 0) continue;
            }
            // current pixel face
            glm::vec3 points[4];
            points[0] = { i, j, curReHeight };  // left and down point
            points[1] = { i + 1, j, curReHeight };
            points[2] = { i + 1, j + 1, curReHeight };
            points[3] = { i, j + 1, curReHeight };
            int list[] = { 0,1,2,0,2,3 };
            for (int k = 0; k < 6; k++)
            {
                int listK = list[k];
                VertexAttribute va{ points[listK] };
                voxelTriModel.vertices.emplace_back(va);
                voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + k);
            }
            voxelTriModel.nVertices += 6;
            voxelTriModel.nIndices += 6;

            // left pixel
            if (leftReHeight < curReHeight)
            {
                glm::vec3 points[4];
                points[0] = { i, j + 1, leftReHeight };  // left and down point
                points[1] = { i, j, leftReHeight };
                points[2] = { i, j, curReHeight };
                points[3] = { i, j + 1, curReHeight };

                int list[] = { 0,1,2,0,2,3 };
                for (int k = 0; k < 6; k++)
                {
                    int listK = list[k];
                    VertexAttribute va{ points[listK] };
                    voxelTriModel.vertices.emplace_back(va);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + k);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }

            // right pixel
            if (rightReHeight < curReHeight)
            {
                glm::vec3 points[4];
                points[0] = { i + 1, j, rightReHeight };  // left and down point
                points[1] = { i + 1, j + 1, rightReHeight };
                points[2] = { i + 1, j + 1, curReHeight };
                points[3] = { i + 1, j, curReHeight };

                int list[] = { 0,1,2,0,2,3 };
                for (int k = 0; k < 6; k++)
                {
                    int listK = list[k];
                    VertexAttribute va{ points[listK] };
                    voxelTriModel.vertices.emplace_back(va);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + k);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }

            // up pixel
            if (upReHeight < curReHeight)
            {
                glm::vec3 points[4];
                points[0] = { i + 1, j, upReHeight };  // left and down point
                points[1] = { i, j, upReHeight };
                points[2] = { i, j, curReHeight };
                points[3] = { i + 1, j, curReHeight };

                int list[] = { 0,2,1,0,3,2 };
                for (int k = 0; k < 6; k++)
                {
                    int listK = list[k];
                    VertexAttribute va{ points[listK] };
                    voxelTriModel.vertices.emplace_back(va);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + k);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }

            // down face
            if (downReHeight < curReHeight)
            {
                glm::vec3 points[4];
                points[0] = { i + 1, j + 1, downReHeight };  // left and down point
                points[1] = { i, j + 1, downReHeight };
                points[2] = { i, j + 1, curReHeight };
                points[3] = { i + 1, j + 1, curReHeight };

                int list[] = { 0,1,2,0,2,3 };
                for (int k = 0; k < 6; k++)
                {
                    int listK = list[k];
                    VertexAttribute va{ points[listK] };
                    voxelTriModel.vertices.emplace_back(va);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + k);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }
        }
    }

    delete[] heightScanline;
    //delete heightDataset;
    GDALClose(heightDataset);
    delete[] minReHeightScanline;

    return voxelTriModel;
}


PrimMesh VoxelDesigner::createTriEntitiesFromTif_roof(std::string heightPath, glm::vec3 targetSize, float stepSize) {
    PrimMesh voxelTriModel;
    voxelTriModel.nVertices=0;
    voxelTriModel.nIndices = 0;
    voxelTriModel.meshId = 0;
    //===================================================================
    /*! height map
     */
//    CPLSetConfigOption("gdal_filename_is_utf8", "no");
//    GDALAllRegister();
//
//    GDALDataset* heightDataset;
//    heightDataset = (GDALDataset*)GDALOpen(heightPath.c_str(), GA_ReadOnly);
//    if (heightDataset == NULL)
//    {
//        std::cout << "fail in open height files!" << std::endl;
//        return voxelTriModel;
//    }
//    int nImgSizeX = heightDataset->GetRasterXSize();
//    int nImgSizeY = heightDataset->GetRasterYSize();
//    int bandcount = heightDataset->GetRasterCount();
//    FLOAT* heightScanline = new FLOAT[nImgSizeX * nImgSizeY];
//    heightDataset->RasterIO(GF_Read, 0, 0, nImgSizeX, nImgSizeY, heightScanline, nImgSizeX, nImgSizeY, GDT_Float32, bandcount, 0, 0, 0, 0);

    std::vector<float> heightScanline0;
    int width,height, nband;
    Utils::readImageinout1(heightPath,heightScanline0,width,height,nband);
    cv::Mat heightScanline0_cv = Utils::convertVector2Mat<float>(heightScanline0,1,height);
    cv::Mat heightScanline_cv;

    int nImgSizeX = targetSize.x;
    int nImgSizeY = targetSize.z;
    cv::resize(heightScanline0_cv,heightScanline_cv,cv::Size(nImgSizeX,nImgSizeY));
    std::vector<float> heightScanline;
    heightScanline = Utils::convertMat2Vector<float>(heightScanline_cv);
    heightScanline0.clear();
    heightScanline0_cv.release();
    heightScanline_cv.release();

    //===================================================================
    /*! create every model through its height: get the center points
     * (this center points is actually the minimum(left and down) point)
     * for edge pixel: create the vertical voxels from the lowest height around to its height.
     *              if the height of this pixel is the lowest, then establish its corresponding voxel height.
     * for non-edge pixel: establish its corresponding voxel height.
     * ENVI tif coordinate system:
     *      .--------> x
     *      |---------
     *      |----.(i, j)  (j * nImgX + i)
     *      |
     *      y
     *
     * spatial relationship:
     *      .------------------------------------------------> x
     *      |----------------- .up (back)----------------------
     *      |--- .left (left)| .cur (center) | .right (right)----
     *      |----------------- .down (forward) ----------------
     *      |-------------------------------------------------
     *      y
     */

    FLOAT* minReHeightScanline = new FLOAT[nImgSizeX * nImgSizeY];

    float minReHeight, rightReHeight, leftReHeight, upReHeight, downReHeight;
    for (int i = 1; i < nImgSizeX - 1; i++)
    {
        for (int j = 1; j < nImgSizeY - 1; j++)
        {
            // all the height
            float curReHeight = std::round(heightScanline[j * nImgSizeX + i] / stepSize);


            leftReHeight = std::round(heightScanline[j * nImgSizeX + i - 1] / stepSize);
            rightReHeight = std::round(heightScanline[j * nImgSizeX + i + 1] / stepSize);
            upReHeight = std::round(heightScanline[(j - 1) * nImgSizeX + i] / stepSize);
            downReHeight = std::round(heightScanline[(j + 1) * nImgSizeX + i] / stepSize);
            // resized height
            minReHeight = std::min({ curReHeight, leftReHeight, rightReHeight, upReHeight, downReHeight });

            int faceId = 0;
            if (downReHeight == minReHeight) faceId = 1;    // forward
            if (upReHeight == minReHeight) faceId = 4;      // backward
            if (leftReHeight == minReHeight) faceId = 3;    // left
            if (rightReHeight == minReHeight) faceId = 2;   // right
            if (curReHeight == minReHeight) faceId = 5;     // up

            minReHeightScanline[j * nImgSizeX + i] = minReHeight;
            //===================================================================
            /*! save [voxelIds: left-down point] and [faceIds] values: NanoVDB attribution location
             * {}
             */
            if (curReHeight == minReHeight)
            {
                if (curReHeight == 0) continue;

                voxelTriModel.voxelIds.emplace_back(i, j, curReHeight - 1);
                voxelTriModel.faceIds.emplace_back(faceId);

                // isValids: 0 forward, 1 backward, 2 left, 3 right, 4 up
                int5 curIsValids = { 0, 0, 0, 0, 1 };
                voxelTriModel.isValids.emplace_back(curIsValids);
            }
            else
            {
                // only the roof voxel is added;
                for (int heightt = curReHeight - 1; heightt < curReHeight; heightt++)
                {
                    voxelTriModel.voxelIds.emplace_back(i, j, heightt);
                    voxelTriModel.faceIds.emplace_back(faceId);

                    // isValids: 0 forward, 1 backward, 2 left, 3 right, 4 up
                    int5 curIsValids = { 0, 0, 0, 0, 1 };
                    if (heightt > downReHeight) curIsValids.values[0] = 1;
                    if (heightt > upReHeight) curIsValids.values[1] = 1;
                    if (heightt > leftReHeight) curIsValids.values[2] = 1;
                    if (heightt > rightReHeight) curIsValids.values[3] = 1;
                    voxelTriModel.isValids.emplace_back(curIsValids);
                }
            }

            //===================================================================
            /*! save [vertices] and [indices] values: mesh
             * because add voxel wastes the memory, so add each face
             */
            // because add voxel wastes the memory, so add each face
            if (curReHeight == minReHeight)    // add up face
            {
                if (curReHeight == 0) continue;
            }
            // current pixel face
            glm::vec3 points[4];
            points[0] = { i, j, curReHeight };  // left and down point
            points[1] = { i + 1, j, curReHeight };
            points[2] = { i + 1, j + 1, curReHeight };
            points[3] = { i, j + 1, curReHeight };
            int list[] = { 0,1,2,0,2,3 };
            for (int k = 0; k < 6; k++)
            {
                int listK = list[k];
                VertexAttribute va{ points[listK] };
                voxelTriModel.vertices.emplace_back(va);
                voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + k);
            }
            voxelTriModel.nVertices += 6;
            voxelTriModel.nIndices += 6;


        }
    }

   // delete[] heightScanline;
    //delete heightDataset;
   // GDALClose(heightDataset);
    heightScanline.clear();
    delete[] minReHeightScanline;

    return voxelTriModel;
}

PrimMesh VoxelDesigner::createTriEntitiesFromTif_wall(std::string heightPath, glm::vec3 targetSize, float stepSize) {
    PrimMesh voxelTriModel;
    voxelTriModel.nVertices=0;
    voxelTriModel.nIndices = 0;
    voxelTriModel.meshId = 0;
    //===================================================================
    /*! height map
     */
//    CPLSetConfigOption("gdal_filename_is_utf8", "no");
//    GDALAllRegister();
//
//    GDALDataset* heightDataset;
//    heightDataset = (GDALDataset*)GDALOpen(heightPath.c_str(), GA_ReadOnly);
//    if (heightDataset == NULL)
//    {
//        std::cout << "fail in open height files!" << std::endl;
//        return voxelTriModel;
//    }
//    int nImgSizeX = heightDataset->GetRasterXSize();
//    int nImgSizeY = heightDataset->GetRasterYSize();
//    int bandcount = heightDataset->GetRasterCount();
//    FLOAT* heightScanline = new FLOAT[nImgSizeX * nImgSizeY];
//    heightDataset->RasterIO(GF_Read, 0, 0, nImgSizeX, nImgSizeY, heightScanline, nImgSizeX, nImgSizeY, GDT_Float32, bandcount, 0, 0, 0, 0);


     std::vector<float> heightScanline0;
     int width,height, nband;
     Utils::readImageinout1(heightPath,heightScanline0,width,height,nband);
     cv::Mat heightScanline0_cv = Utils::convertVector2Mat<float>(heightScanline0,1,height);
    cv::Mat heightScanline_cv;

    int nImgSizeX = targetSize.x;
    int nImgSizeY = targetSize.z;
    cv::resize(heightScanline0_cv,heightScanline_cv,cv::Size(nImgSizeX,nImgSizeY));
    std::vector<float> heightScanline;
    heightScanline = Utils::convertMat2Vector<float>(heightScanline_cv);
    heightScanline0.clear();
    heightScanline0_cv.release();
    heightScanline_cv.release();



    //===================================================================
    /*! create every model through its height: get the center points
     * (this center points is actually the minimum(left and down) point)
     * for edge pixel: create the vertical voxels from the lowest height around to its height.
     *              if the height of this pixel is the lowest, then establish its corresponding voxel height.
     * for non-edge pixel: establish its corresponding voxel height.
     * ENVI tif coordinate system:
     *      .--------> x
     *      |---------
     *      |----.(i, j)  (j * nImgX + i)
     *      |
     *      y
     *
     * spatial relationship:
     *      .------------------------------------------------> x
     *      |----------------- .up (back)----------------------
     *      |--- .left (left)| .cur (center) | .right (right)----
     *      |----------------- .down (forward) ----------------
     *      |-------------------------------------------------
     *      y
     */

    FLOAT* minReHeightScanline = new FLOAT[nImgSizeX * nImgSizeY];

    float minReHeight, rightReHeight, leftReHeight, upReHeight, downReHeight;
    for (int i = 1; i < nImgSizeX - 1; i++)
    {
        for (int j = 1; j < nImgSizeY - 1; j++)
        {
            // all the height
            float curReHeight = std::round(heightScanline[j * nImgSizeX + i] / stepSize);

            if(curReHeight>0)
            {
                int a = 10;
            }

            leftReHeight = std::round(heightScanline[j * nImgSizeX + i - 1] / stepSize);
            rightReHeight = std::round(heightScanline[j * nImgSizeX + i + 1] / stepSize);
            upReHeight = std::round(heightScanline[(j - 1) * nImgSizeX + i] / stepSize);
            downReHeight = std::round(heightScanline[(j + 1) * nImgSizeX + i] / stepSize);
            // resized height
            minReHeight = std::min({ curReHeight, leftReHeight, rightReHeight, upReHeight, downReHeight });

            int faceId = 0;
            if (downReHeight == minReHeight) faceId = 1;    // forward
            if (upReHeight == minReHeight) faceId = 4;      // backward
            if (leftReHeight == minReHeight) faceId = 3;    // left
            if (rightReHeight == minReHeight) faceId = 2;   // right
            if (curReHeight == minReHeight) faceId = 5;     // up

            minReHeightScanline[j * nImgSizeX + i] = minReHeight;
            //===================================================================
            /*! save [voxelIds: left-down point] and [faceIds] values: NanoVDB attribution location
             * {}
             */
            if (curReHeight == minReHeight)
            {
                if (curReHeight == 0) continue;

                voxelTriModel.voxelIds.emplace_back(i, j, curReHeight - 1);
                voxelTriModel.faceIds.emplace_back(faceId);

                // isValids: center 0, 0 forward, 1 backward, 2 left, 3 right, 4 up
                int5 curIsValids = { 0, 0, 0, 0, 1 };
                voxelTriModel.isValids.emplace_back(curIsValids);
            }
            else
            {
                for (int heighti = 0; heighti < (curReHeight - minReHeight); heighti++)
                {
                    voxelTriModel.voxelIds.emplace_back(i, j, minReHeight + heighti);
                    voxelTriModel.faceIds.emplace_back(faceId);

                    // isValids: 0 forward, 1 backward, 2 left, 3 right, 4 up
                    int5 curIsValids = { 0, 0, 0, 0, 1 };
                    if (minReHeight + heighti > downReHeight) curIsValids.values[0] = 1;
                    if (minReHeight + heighti > upReHeight) curIsValids.values[1] = 1;
                    if (minReHeight + heighti > leftReHeight) curIsValids.values[2] = 1;
                    if (minReHeight + heighti > rightReHeight) curIsValids.values[3] = 1;
                    voxelTriModel.isValids.emplace_back(curIsValids);
                }
            }

            //===================================================================
            /*! save [vertices] and [indices] values: mesh
             * because add voxel wastes the memory, so add each face
             */
            // because add voxel wastes the memory, so add each face
            if (curReHeight == minReHeight)    // add up face
            {
                if (curReHeight == 0) continue;
            }


            // left pixel
            if (leftReHeight < curReHeight)
            {
                glm::vec3 points[4];
                points[0] = { i, j + 1, leftReHeight };  // left and down point
                points[1] = { i, j, leftReHeight };
                points[2] = { i, j, curReHeight };
                points[3] = { i, j + 1, curReHeight };

                int list[] = { 0,1,2,0,2,3 };
                for (int k = 0; k < 6; k++)
                {
                    int listK = list[k];
                    VertexAttribute va{ points[listK] };
                    voxelTriModel.vertices.emplace_back(va);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + k);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }

            // right pixel
            if (rightReHeight < curReHeight)
            {
                glm::vec3 points[4];
                points[0] = { i + 1, j, rightReHeight };  // left and down point
                points[1] = { i + 1, j + 1, rightReHeight };
                points[2] = { i + 1, j + 1, curReHeight };
                points[3] = { i + 1, j, curReHeight };

                int list[] = { 0,1,2,0,2,3 };
                for (int k = 0; k < 6; k++)
                {
                    int listK = list[k];
                    VertexAttribute va{ points[listK] };
                    voxelTriModel.vertices.emplace_back(va);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + k);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }

            // up pixel
            if (upReHeight < curReHeight)
            {
                glm::vec3 points[4];
                points[0] = { i + 1, j, upReHeight };  // left and down point
                points[1] = { i, j, upReHeight };
                points[2] = { i, j, curReHeight };
                points[3] = { i + 1, j, curReHeight };

                int list[] = { 0,2,1,0,3,2 };
                for (int k = 0; k < 6; k++)
                {
                    int listK = list[k];
                    VertexAttribute va{ points[listK] };
                    voxelTriModel.vertices.emplace_back(va);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + k);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }

            // down face
            if (downReHeight < curReHeight)
            {
                glm::vec3 points[4];
                points[0] = { i + 1, j + 1, downReHeight };  // left and down point
                points[1] = { i, j + 1, downReHeight };
                points[2] = { i, j + 1, curReHeight };
                points[3] = { i + 1, j + 1, curReHeight };

                int list[] = { 0,1,2,0,2,3 };
                for (int k = 0; k < 6; k++)
                {
                    int listK = list[k];
                    VertexAttribute va{ points[listK] };
                    voxelTriModel.vertices.emplace_back(va);
                    voxelTriModel.indices.emplace_back(voxelTriModel.nVertices + k);
                }

                voxelTriModel.nVertices += 6;
                voxelTriModel.nIndices += 6;
            }
        }
    }

    //delete[] heightScanline;
    //delete heightDataset;
   // GDALClose(heightDataset);
    heightScanline.clear();
    delete[] minReHeightScanline;

    return voxelTriModel;
}

