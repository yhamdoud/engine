#include <iostream>
#include <memory>
#include <optional>
#include <vector>

#define CGLTF_IMPLEMENTATION

#include <cgltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "model.hpp"

using std::optional;
using std::unique_ptr;
using std::vector;

using glm::mat4;
using glm::vec2;
using glm::vec3;

using namespace engine;

vector<uint32_t> process_index_accessor(const cgltf_accessor &accessor)
{
    if (accessor.is_sparse)
        throw std::invalid_argument("Accessor is sparse");

    if (accessor.type != cgltf_type_scalar)
        throw std::invalid_argument("Unsupported index type");

    cgltf_buffer_view *bufferView = accessor.buffer_view;
    if (!bufferView)
        throw std::invalid_argument("Missing index data");

    cgltf_buffer *buffer = bufferView->buffer;
    if (!buffer)
        throw std::invalid_argument("Missing index data");

    cgltf_size offset = accessor.offset + accessor.buffer_view->offset;

    if (accessor.component_type == cgltf_component_type_r_32u)
    {
        uint32_t *data = (uint32_t *)((uint8_t *)buffer->data + offset);
        vector<uint32_t> indices(data, data + accessor.count);
        return indices;
    }
    else if (accessor.component_type == cgltf_component_type_r_16u)
    {
        // FIXME: Offset is in bytes.
        uint16_t *data = (uint16_t *)((uint8_t *)buffer->data + offset);
        vector<uint32_t> indices(data, data + accessor.count);
        return indices;
    }
    else
    {
        throw std::invalid_argument("Unsupported index type");
    }
}

static Mesh process_triangles(const cgltf_primitive triangles)
{
    vector<vec3> positions;
    vector<vec3> normals;
    vector<vec2> tex_coords;

    for (size_t i = 0; i < triangles.attributes_count; i++)
    {
        auto &attribute = triangles.attributes[i];
        auto accessor = attribute.data;

        switch (attribute.type)
        {
        case cgltf_attribute_type_position:
        {
            cgltf_size float_count =
                cgltf_accessor_unpack_floats(accessor, nullptr, 0);

            positions.resize(accessor->count);
            cgltf_accessor_unpack_floats(
                accessor, reinterpret_cast<float *>(positions.data()),
                float_count);

            break;
        }
        case cgltf_attribute_type_normal:
        {
            cgltf_size float_count =
                cgltf_accessor_unpack_floats(accessor, nullptr, 0);

            normals.resize(accessor->count);
            cgltf_accessor_unpack_floats(
                accessor, reinterpret_cast<float *>(normals.data()),
                float_count);
            break;
        }
        case cgltf_attribute_type_texcoord:
        {
            cgltf_size float_count =
                cgltf_accessor_unpack_floats(accessor, nullptr, 0);

            tex_coords.resize(accessor->count);
            cgltf_accessor_unpack_floats(
                accessor, reinterpret_cast<float *>(tex_coords.data()),
                float_count);
            break;
        }
        case cgltf_attribute_type_invalid:
        case cgltf_attribute_type_tangent:
        case cgltf_attribute_type_color:
        case cgltf_attribute_type_joints:
        case cgltf_attribute_type_weights:
            std::cerr << "Unsupported triangle attribute" << std::endl;
            break;
        }
    }

    auto indices = process_index_accessor(*triangles.indices);

    return Mesh{positions, normals, tex_coords, indices};
}

static void process_mesh(const cgltf_mesh &mesh, vector<Mesh> &meshes)
{
    for (size_t i = 0; i < mesh.primitives_count; i++)
    {
        auto &primitive = mesh.primitives[i];

        switch (primitive.type)
        {
        case cgltf_primitive_type_triangles:
        {
            meshes.push_back(process_triangles(primitive));
            break;
        }
        case cgltf_primitive_type_points:
        case cgltf_primitive_type_lines:
        case cgltf_primitive_type_line_loop:
        case cgltf_primitive_type_line_strip:
        case cgltf_primitive_type_triangle_strip:
        case cgltf_primitive_type_triangle_fan:
            std::cerr << "Unsupported primitive type" << std::endl;
            break;
        }
    }
}

static void process_node(const cgltf_node &node, vector<Mesh> &meshes)
{
    mat4 local_transform;
    cgltf_node_transform_local(&node, glm::value_ptr(local_transform));

    if (node.mesh)
        process_mesh(*node.mesh, meshes);

    for (size_t j = 0; j < node.children_count; j++)
        process_node(*node.children[j], meshes);
}

static void process_scene(const cgltf_scene &scene, vector<Mesh> &meshes)
{
    for (size_t i = 0; i < scene.nodes_count; i++)
        process_node(*scene.nodes[i], meshes);
}

vector<Mesh> Mesh::from_gtlf(const std::filesystem::path &path)
{
    vector<Mesh> meshes;

    cgltf_options options = {};
    cgltf_data *data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);

    if (result == cgltf_result_success)
        result = cgltf_load_buffers(&options, data, path.c_str());

    if (result == cgltf_result_success)
        result = cgltf_validate(data);

    if (result == cgltf_result_success)
        for (size_t i = 0; i < data->scenes_count; i++)
            process_scene(data->scenes[i], meshes);

    cgltf_free(data);
    return meshes;
}
