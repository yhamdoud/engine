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

static Sampler process_sampler(const cgltf_sampler &sampler)
{
    return Sampler{sampler.mag_filter, sampler.min_filter, sampler.wrap_s,
                   sampler.wrap_t};
}

static optional<Texture>
process_texture_view(const cgltf_texture_view &texture_view)
{
    if (texture_view.texture == nullptr)
        return std::nullopt;

    const auto &texture = *texture_view.texture;
    const auto &image = *texture.image;

    auto sampler = texture.sampler == nullptr
                       ? Sampler{}
                       : process_sampler(*texture.sampler);

    if (image.buffer_view)
    {
        const auto &buffer_view = *image.buffer_view;
        auto const &buffer = *buffer_view.buffer;

        int x, y, c;
        uint8_t *buffer_data = (uint8_t *)buffer.data + buffer_view.offset;

        return Texture::from_memory(
            buffer_data, static_cast<int>(buffer_view.size), sampler);
    }
    else
    {
        logger.warn("Texture data is in file.");
    }

    return std::nullopt;
}

static Material process_material(const cgltf_material &gltf_material)
{
    Material material;

    material.normal = process_texture_view(gltf_material.normal_texture);
    //    material.emissive =
    //    process_texture_view(gltf_material.emissive_texture);
    //    material.occlusion =
    //    process_texture_view(gltf_material.occlusion_texture);

    if (gltf_material.has_pbr_metallic_roughness)
    {
        const auto &gltf_pbr = gltf_material.pbr_metallic_roughness;
        material.base_color = process_texture_view(gltf_pbr.base_color_texture);
        material.metallic_roughness =
            process_texture_view(gltf_pbr.metallic_roughness_texture);
        material.base_color_factor = make_vec4(gltf_pbr.base_color_factor);
        material.metallic_factor = gltf_pbr.metallic_factor;
        material.roughness_factor = gltf_pbr.roughness_factor;

        // FIXME:
        if (material.metallic_roughness)
            material.metallic_roughness->sampler.use_mipmap = true;

        if (material.base_color)
            material.base_color->sampler.use_mipmap = true;
    }
    else if (gltf_material.has_pbr_specular_glossiness)
    {
        logger.warn("Specular glossiness materials aren't supported.");
    }

    return material;
}

template <typename T>
void process_attribute_accessor(const cgltf_accessor &accessor, vector<T> &vec)
{
    cgltf_size float_count =
        cgltf_accessor_unpack_floats(&accessor, nullptr, 0);

    vec.resize(accessor.count);
    cgltf_accessor_unpack_floats(
        &accessor, reinterpret_cast<float *>(vec.data()), float_count);
}

static Model process_triangles(const cgltf_primitive &triangles)
{
    vector<vec3> positions;
    vector<vec3> normals;
    vector<vec2> tex_coords;
    vector<vec4> tangents;

    for (size_t i = 0; i < triangles.attributes_count; i++)
    {
        auto &attribute = triangles.attributes[i];
        auto &accessor = *attribute.data;

        switch (attribute.type)
        {
        case cgltf_attribute_type_position:
        {
            process_attribute_accessor(accessor, positions);
            break;
        }
        case cgltf_attribute_type_normal:
        {
            process_attribute_accessor(accessor, normals);
            break;
        }
        case cgltf_attribute_type_texcoord:
        {
            process_attribute_accessor(accessor, tex_coords);
            break;
        }
        case cgltf_attribute_type_tangent:
        {
            if (accessor.component_type != cgltf_component_type_r_32f &&
                accessor.type != cgltf_type_vec4)
            {
                logger.warn("Unsupported tangent attribute type or component.");
                break;
            }

            process_attribute_accessor(accessor, tangents);
            break;
        }
        case cgltf_attribute_type_color:
        case cgltf_attribute_type_joints:
        case cgltf_attribute_type_weights:
            logger.warn("Unsupported triangle attribute.");
            break;
        case cgltf_attribute_type_invalid:
            logger.error("Invalid triangle attribute.");
            break;
        }
    }

    auto indices = process_index_accessor(*triangles.indices);

    return Model{
        make_unique<Mesh>(
            Mesh{positions, normals, tex_coords, tangents, indices}),
        (triangles.material) ? process_material(*triangles.material)
                             : Material{},
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
                 std::unique_ptr<uint8_t, void (*)(void *)> data,
                 Sampler sampler)
    : width{width}, height{height},
      component_count{component_count}, data{std::move(data)}, sampler{sampler}
{
}

optional<Texture> Texture::from_file(path path, Sampler sampler)
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
        unique_ptr<uint8_t, void (*)(void *)>(data, stbi_image_free), sampler);
}

optional<Texture> Texture::from_memory(uint8_t *data, int length,
                                       Sampler sampler)
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
        unique_ptr<uint8_t, void (*)(void *)>(image_data, stbi_image_free),
        sampler);
}