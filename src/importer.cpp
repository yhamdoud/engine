#include <cstddef>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <optional>

#include "constants.hpp"
#include "importer.hpp"
#include "logger.hpp"
#include "model.hpp"

using namespace engine;
using namespace std;

GltfImporter::GltfImporter(const std::filesystem::path &path,
                           Renderer &renderer)
    : path(path), renderer(renderer)
{
}

optional<ImportError> GltfImporter::import()
{
    if (!filesystem::exists(path))
        return ImportError::invalid_path;

    folder = path.parent_path();

    logger.info("Loading model at path: {}", path.string());

    cgltf_options options = {};
    cgltf_result result =
        cgltf_parse_file(&options, path.string().c_str(), &gltf);

    if (result == cgltf_result_success)
        result = cgltf_load_buffers(&options, gltf, path.string().c_str());

    if (result == cgltf_result_success)
        result = cgltf_validate(gltf);

    images.resize(gltf->images_count, invalid_texture_id);

    if (result == cgltf_result_success)
        for (size_t i = 0; i < gltf->scenes_count; i++)
            process_scene(gltf->scenes[i]);

    cgltf_free(gltf);

    logger.info("Finished loading model at path: {}", path.string());

    return nullopt;
}

void GltfImporter::process_scene(const cgltf_scene &scene)
{

    for (size_t i = 0; i < scene.nodes_count; i++)
        process_node(*scene.nodes[i]);
}

void GltfImporter::process_node(const cgltf_node &node)
{
    glm::mat4 local_transform;
    // FIXME:
    cgltf_node_transform_world(&node, glm::value_ptr(local_transform));

    if (node.mesh)
        process_mesh(*node.mesh, local_transform);

    for (size_t j = 0; j < node.children_count; j++)
        process_node(*node.children[j]);
}

void GltfImporter::process_mesh(const cgltf_mesh &mesh,
                                const glm::mat4 transform)
{
    for (size_t i = 0; i < mesh.primitives_count; i++)
    {
        auto &primitive = mesh.primitives[i];

        switch (primitive.type)
        {
        case cgltf_primitive_type_triangles:
        {
            auto m = process_triangles(primitive);
            m.model = transform;
            models.emplace_back(move(m));
            break;
        }
        case cgltf_primitive_type_points:
        case cgltf_primitive_type_lines:
        case cgltf_primitive_type_line_loop:
        case cgltf_primitive_type_line_strip:
        case cgltf_primitive_type_triangle_strip:
        case cgltf_primitive_type_triangle_fan:
            logger.error("Unsupported primitive type");
            break;
        }
    }
}

Entity GltfImporter::process_triangles(const cgltf_primitive &triangles)
{
    vector<glm::vec3> positions;
    vector<glm::vec3> normals;
    vector<glm::vec2> tex_coords;
    vector<glm::vec4> tangents;

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

    vector<Vertex> vertices(positions.size());

    if (tangents.size() == 0)
    {
        logger.error("missing tangents");
        for (size_t i = 0; i < vertices.size(); i++)
            vertices[i] =
                Vertex{positions[i], normals[i], tex_coords[i], glm::vec4(0)};
    }
    else
    {
        for (size_t i = 0; i < vertices.size(); i++)
            vertices[i] =
                Vertex{positions[i], normals[i], tex_coords[i], tangents[i]};
    }

    auto indices = process_index_accessor(*triangles.indices);

    size_t mesh_idx =
        renderer.register_mesh(Mesh{move(vertices), move(indices)});

    return Entity{
        Entity::Flags::casts_shadow,
        mesh_idx,
        glm::mat4(1.),
        triangles.material ? process_material(*triangles.material) : Material{},
    };
}

vector<uint32_t>
GltfImporter::process_index_accessor(const cgltf_accessor &accessor)
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
        auto *data = (uint32_t *)((uint8_t *)buffer->data + offset);
        vector<uint32_t> indices(data, data + accessor.count);
        return indices;
    }
    else if (accessor.component_type == cgltf_component_type_r_16u)
    {
        // FIXME: Offset is in bytes.
        auto *data = (uint16_t *)((uint8_t *)buffer->data + offset);
        vector<uint32_t> indices(data, data + accessor.count);
        return indices;
    }
    else
    {
        throw std::invalid_argument("Unsupported index type");
    }
}

Sampler GltfImporter::process_sampler(const cgltf_sampler &sampler)
{
    return Sampler{
        sampler.mag_filter != GL_NONE ? sampler.mag_filter : GL_LINEAR,
        sampler.min_filter != GL_NONE ? sampler.min_filter : GL_LINEAR,
        sampler.wrap_s,
        sampler.wrap_t,
    };
}

int GltfImporter::get_image_index(cgltf_image *image)
{
    return (int)(image - gltf->images);
}

uint GltfImporter::load_texture_cache(const cgltf_texture_view &texture_view)
{
    if (texture_view.texture == nullptr)
        return invalid_texture_id;

    int idx = get_image_index(texture_view.texture->image);

    // Image was already loaded.
    if (uint cached = images[idx]; cached != invalid_texture_id)
    {
        return cached;
    }

    uint texture = process_texture_view(texture_view);
    images[idx] = texture;

    return texture;
}

uint GltfImporter::process_texture_view(const cgltf_texture_view &texture_view)
{
    const auto &texture = *texture_view.texture;
    const auto &image = *texture.image;

    constexpr bool find_dds = true;

    if (image.uri && find_dds)
    {
        auto tex = gli::load(
            (folder / "dds" /
             std::filesystem::path(image.uri).filename().replace_extension(
                 "dds"))
                .string()
                .c_str());

        if (!tex.empty())
            return get<unsigned int>(renderer.register_texture(tex));
    }

    auto sampler = texture.sampler == nullptr
                       ? Sampler{}
                       : process_sampler(*texture.sampler);

    if (image.buffer_view)
    {
        const auto &buffer_view = *image.buffer_view;
        auto const &buffer = *buffer_view.buffer;

        uint8_t *buffer_data = (uint8_t *)buffer.data + buffer_view.offset;

        return get<uint>(renderer.register_texture(*Texture::from_memory(
            buffer_data, static_cast<int>(buffer_view.size), sampler)));
    }
    else
    {
        return get<uint>(renderer.register_texture(*Texture::from_file(
            std::filesystem::path(folder / image.uri), sampler)));
    }

    return invalid_texture_id;
}

Material GltfImporter::process_material(const cgltf_material &gltf_material)
{
    Material material;

    material.normal = load_texture_cache(gltf_material.normal_texture);
    //    material.emissive =
    //    process_texture_view(gltf_material.emissive_texture);
    //    material.occlusion =
    //    process_texture_view(gltf_material.occlusion_texture);

    material.alpha_mode = (AlphaMode)gltf_material.alpha_mode;
    if (material.alpha_mode == AlphaMode::mask)
        material.alpha_cutoff = gltf_material.alpha_cutoff;

    if (gltf_material.has_pbr_metallic_roughness)
    {
        const auto &gltf_pbr = gltf_material.pbr_metallic_roughness;

        material.base_color = load_texture_cache(gltf_pbr.base_color_texture);
        material.metallic_roughness =
            load_texture_cache(gltf_pbr.metallic_roughness_texture);

        if (material.base_color == invalid_texture_id)
            material.base_color_factor =
                glm::make_vec4(gltf_pbr.base_color_factor);

        material.metallic_factor = gltf_pbr.metallic_factor;
        material.roughness_factor = gltf_pbr.roughness_factor;
    }
    else if (gltf_material.has_pbr_specular_glossiness)
    {
        logger.warn("Specular glossiness materials aren't supported.");
    }

    return material;
}
