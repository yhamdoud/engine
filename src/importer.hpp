#pragma once

#include <filesystem>

#include "model.hpp"
#include "renderer/renderer.hpp"

namespace engine
{

class GltfImporter
{
    std::vector<uint> images;

    cgltf_data *gltf = nullptr;

    std::filesystem::path path;
    std::filesystem::path folder;
    Renderer &renderer;

    int get_image_index(cgltf_image *image);

    void process_scene(const cgltf_scene &scene);
    void process_node(const cgltf_node &node);
    void process_mesh(const cgltf_mesh &mesh, const glm::mat4 transform);
    Entity process_triangles(const cgltf_primitive &triangles);
    Material process_material(const cgltf_material &gltf_material);
    uint process_texture_view(const cgltf_texture_view &texture_view);
    uint load_texture_cache(const cgltf_texture_view &texture_view);
    Sampler process_sampler(const cgltf_sampler &sampler);

    std::vector<uint32_t>
    process_index_accessor(const cgltf_accessor &accessor);

    template <typename T>
    void process_attribute_accessor(const cgltf_accessor &accessor,
                                    std::vector<T> &vec);

  public:
    std::vector<Entity> models;
    GltfImporter(const std::filesystem::path &path, Renderer &renderer);
    std::optional<ImportError> import();
};

template <typename T>
void GltfImporter::process_attribute_accessor(const cgltf_accessor &accessor,
                                              std::vector<T> &vec)
{
    cgltf_size float_count =
        cgltf_accessor_unpack_floats(&accessor, nullptr, 0);

    vec.resize(accessor.count);
    cgltf_accessor_unpack_floats(
        &accessor, reinterpret_cast<float *>(vec.data()), float_count);
}

} // namespace engine