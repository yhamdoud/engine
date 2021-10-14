#define CGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <variant>

#include <iostream>
#include <memory>
#include <optional>
#include <vector>

#include "gli/format.hpp"
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <stb_image.h>
#include <stb_image_write.h>

#include "logger.hpp"
#include "model.hpp"

using namespace std;

using filesystem::path;

using namespace engine;

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

    uint8_t *data = stbi_load(path.string().c_str(), &width, &height, &channel_count, 0);
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