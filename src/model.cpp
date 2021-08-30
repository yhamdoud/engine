#define CGLTF_IMPLEMENTATION

#include <iostream>
#include <memory>
#include <optional>
#include <vector>

#include <cgltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

#include "logger.hpp"
#include "model.hpp"

using namespace std;
using namespace glm;

using filesystem::path;

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

static Texture process_texture_view(const cgltf_texture_view &texture_view)
{
    const auto &texture = *texture_view.texture;
    const auto &image = texture.image;

    if (image->buffer_view)
    {
        const auto &buffer_view = *image->buffer_view;

        int x, y, c;

        auto const &buffer = *buffer_view.buffer;
        uint8_t *buffer_data = (uint8_t *)buffer.data + buffer_view.offset;

        return *Texture::from_memory(buffer_data,
                                     static_cast<int>(buffer_view.size));
    }
    else
    {
        logger.warn("Texture data is in file.");
        abort();
    }
}

static Texture process_material(const cgltf_material &material)
{

    if (material.has_pbr_metallic_roughness)
    {
        const auto &metallic_roughness = material.pbr_metallic_roughness;
        return process_texture_view(metallic_roughness.base_color_texture);
    }
    else if (material.has_pbr_specular_glossiness)
    {
        logger.warn("Specular glossiness materials aren't supported.");
        abort();
    }
}

static Model process_triangles(const cgltf_primitive &triangles)
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

    return Model{
        make_unique<Mesh>(Mesh{positions, normals, tex_coords, indices}),
        triangles.material
            ? make_unique<Texture>(process_material(*triangles.material))
            : nullptr,
    };
}

static void process_mesh(const cgltf_mesh &mesh, vector<Model> &models,
                         const mat4 transform)
{
    for (size_t i = 0; i < mesh.primitives_count; i++)
    {
        auto &primitive = mesh.primitives[i];

        switch (primitive.type)
        {
        case cgltf_primitive_type_triangles:
        {
            auto m = process_triangles(primitive);
            m.transform = transform;
            models.emplace_back(move(m));
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

static void process_node(const cgltf_node &node, vector<Model> &meshes)
{
    mat4 local_transform;
    // FIXME:
    cgltf_node_transform_world(&node, glm::value_ptr(local_transform));

    if (node.mesh)
        process_mesh(*node.mesh, meshes, local_transform);

    for (size_t j = 0; j < node.children_count; j++)
        process_node(*node.children[j], meshes);
}

static void process_scene(const cgltf_scene &scene, vector<Model> &models)
{
    for (size_t i = 0; i < scene.nodes_count; i++)
        process_node(*scene.nodes[i], models);
}

vector<Model> engine::load_gltf(const path &path)
{
    logger.info("Loading model at path: {}", path.string());

    vector<Model> models;

    cgltf_options options = {};
    cgltf_data *data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);

    if (result == cgltf_result_success)
        result = cgltf_load_buffers(&options, data, path.c_str());

    if (result == cgltf_result_success)
        result = cgltf_validate(data);

    if (result == cgltf_result_success)
        for (size_t i = 0; i < data->scenes_count; i++)
            process_scene(data->scenes[i], models);

    cgltf_free(data);

    logger.info("Finished loading model at path: {}", path.string());

    return models;
}

Texture::Texture(int width, int height, int component_count,
                 std::unique_ptr<uint8_t, void (*)(void *)> data)
    : width{width}, height{height},
      component_count{component_count}, data{std::move(data)}
{
}

optional<Texture> Texture::from_file(path path)
{
    int width, height, channel_count;

    uint8_t *data = stbi_load(path.c_str(), &width, &height, &channel_count, 0);
    if (data == nullptr)
    {
        logger.error("Error loading texture at path: {}", path.string());
        return nullopt;
    }

    return make_optional<Texture>(
        width, height, channel_count,
        unique_ptr<uint8_t, void (*)(void *)>(data, stbi_image_free));
}

optional<Texture> Texture::from_memory(uint8_t *data, int length)
{
    int width, height, channel_count;
    uint8_t *image_data =
        stbi_load_from_memory(data, length, &width, &height, &channel_count, 0);

    if (image_data == nullptr)
    {
        logger.error("Error loading texture from memory.");
        return nullopt;
    }

    return make_optional<Texture>(
        width, height, channel_count,
        unique_ptr<uint8_t, void (*)(void *)>(image_data, stbi_image_free));
}