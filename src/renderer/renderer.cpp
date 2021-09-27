#include <limits>

#include <array>
#include <numeric>
#include <string>

#include <glm/ext.hpp>
#include <glm/gtx/string_cast.hpp>
#include <stb_image_write.h>

#include "constants.hpp"
#include "logger.hpp"
#include "model.hpp"
#include "primitives.hpp"
#include "renderer.hpp"

#include <Tracy.hpp>
#include <TracyOpenGL.hpp>

using namespace glm;
using namespace std;

using namespace engine;

using std::filesystem::path;

constexpr uint default_framebuffer = 0;

uint binding_positions = 0;
uint binding_normals = 1;
uint binding_tex_coords = 2;
uint binding_tangents = 3;

int attrib_positions = 0;
int attrib_normals = 1;
int attrib_tex_coords = 2;
int attrib_tangents = 3;

Renderer::Renderer(glm::ivec2 viewport_size, uint skybox_texture)
{
    ctx_v.size = viewport_size;
    ctx_r.skybox_tex = skybox_texture;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_CULL_FACE);

    glClearColor(0.f, 0.f, 0.f, 1.0f);

    // Entity stuff.
    {
        glCreateVertexArrays(1, &ctx_r.entity_vao);

        glEnableVertexArrayAttrib(ctx_r.entity_vao, attrib_positions);
        glVertexArrayAttribFormat(ctx_r.entity_vao, attrib_positions, 3,
                                  GL_FLOAT, false, 0);
        glVertexArrayAttribBinding(ctx_r.entity_vao, attrib_positions,
                                   binding_positions);

        glEnableVertexArrayAttrib(ctx_r.entity_vao, attrib_normals);
        glVertexArrayAttribFormat(ctx_r.entity_vao, attrib_normals, 3, GL_FLOAT,
                                  false, 0);
        glVertexArrayAttribBinding(ctx_r.entity_vao, attrib_normals,
                                   binding_normals);

        glEnableVertexArrayAttrib(ctx_r.entity_vao, attrib_tex_coords);
        glVertexArrayAttribFormat(ctx_r.entity_vao, attrib_tex_coords, 2,
                                  GL_FLOAT, false, 0);
        glVertexArrayAttribBinding(ctx_r.entity_vao, attrib_tex_coords,
                                   binding_tex_coords);

        glEnableVertexArrayAttrib(ctx_r.entity_vao, attrib_tangents);
        glVertexArrayAttribFormat(ctx_r.entity_vao, attrib_tangents, 4,
                                  GL_FLOAT, false, 0);
        glVertexArrayAttribBinding(ctx_r.entity_vao, attrib_tangents,
                                   binding_tangents);
    }

    // Skybox stuff.
    {
        unsigned int buffer;
        glCreateBuffers(1, &buffer);

        glCreateVertexArrays(1, &ctx_r.skybox_vao);

        unsigned int binding = 0, attribute = 0;

        glVertexArrayAttribFormat(ctx_r.skybox_vao, attribute, 3, GL_FLOAT,
                                  false, 0);
        glVertexArrayAttribBinding(ctx_r.skybox_vao, attribute, binding);
        glEnableVertexArrayAttrib(ctx_r.skybox_vao, attribute);

        glNamedBufferStorage(buffer, sizeof(primitives::cube_verts),
                             primitives::cube_verts.data(), GL_NONE);
        glVertexArrayVertexBuffer(ctx_r.skybox_vao, binding_positions, buffer,
                                  0, sizeof(vec3));
    }

    {
        uint depth;
        glCreateRenderbuffers(1, &depth);
        glNamedRenderbufferStorage(depth, GL_DEPTH_COMPONENT32, ctx_v.size.x,
                                   ctx_v.size.y);

        glCreateTextures(GL_TEXTURE_2D, 1, &ctx_v.hdr_tex);
        glTextureStorage2D(ctx_v.hdr_tex, 1, GL_RGBA16F, ctx_v.size.x,
                           ctx_v.size.y);
        glTextureParameteri(ctx_v.hdr_tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(ctx_v.hdr_tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(ctx_v.hdr_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(ctx_v.hdr_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glCreateFramebuffers(1, &ctx_v.hdr_framebuf);
        glNamedFramebufferTexture(ctx_v.hdr_framebuf, GL_COLOR_ATTACHMENT0,
                                  ctx_v.hdr_tex, 0);
        glNamedFramebufferRenderbuffer(ctx_v.hdr_framebuf, GL_DEPTH_ATTACHMENT,
                                       GL_RENDERBUFFER, depth);

        if (glCheckNamedFramebufferStatus(ctx_v.hdr_framebuf, GL_FRAMEBUFFER) !=
            GL_FRAMEBUFFER_COMPLETE)
            logger.error("Main viewport frame buffer incomplete.");
    }

    {
        auto &ctx = probe_view.ctx;

        uint depth;
        glCreateRenderbuffers(1, &depth);
        glNamedRenderbufferStorage(depth, GL_DEPTH_COMPONENT32, ctx.size.x,
                                   ctx.size.y);

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &ctx.hdr_tex);
        glTextureStorage2D(ctx.hdr_tex, 1, GL_RGBA16F, ctx.size.x, ctx.size.y);
        glTextureParameteri(ctx.hdr_tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(ctx.hdr_tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glCreateFramebuffers(1, &ctx.hdr_framebuf);
        glNamedFramebufferRenderbuffer(ctx.hdr_framebuf, GL_DEPTH_ATTACHMENT,
                                       GL_RENDERBUFFER, depth);

        if (glCheckNamedFramebufferStatus(ctx.hdr_framebuf, GL_FRAMEBUFFER) !=
            GL_FRAMEBUFFER_COMPLETE)
            logger.error("Probe frame buffer incomplete.");
    }

    // Setup state for displaying probe.
    auto sphere_model = std::move(load_gltf(models_path / "sphere.glb")[0]);
    ctx_r.probe_mesh_idx = register_mesh(*sphere_model.mesh);

    shadow.initialize(ctx_v);
    geometry.initialize(ctx_v);
    ssao.initialize(ctx_v);
    lighting.initialize(ctx_v);
    bloom.initialize(ctx_v);
    tone_map.initialize(ctx_v);

    // TODO: move

    probe_view.shadow.initialize(probe_view.ctx);
    probe_view.geometry.initialize(probe_view.ctx);
    probe_view.lighting.initialize(probe_view.ctx);
    probe_view.forward.initialize(probe_view.ctx);
}

Renderer::~Renderer()
{
    // TODO: clean up mesh instances.
}

variant<uint, Renderer::Error>
Renderer::register_texture(const Texture &texture)
{
    uint id;
    glCreateTextures(GL_TEXTURE_2D, 1, &id);

    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER,
                        texture.sampler.magnify_filter);
    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER,
                        texture.sampler.minify_filter);
    glTextureParameteri(id, GL_TEXTURE_WRAP_S, texture.sampler.wrap_s);
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, texture.sampler.wrap_t);

    GLenum internal_format, format;

    switch (texture.component_count)
    {
    case 3:
        internal_format = (texture.sampler.is_srgb) ? GL_SRGB8 : GL_RGB8;
        format = GL_RGB;
        break;
    case 4:
        internal_format = (texture.sampler.is_srgb) ? GL_SRGB8_ALPHA8 : GL_RGB8;
        format = GL_RGBA;
        break;
    default:
        return Renderer::Error::unsupported_texture_format;
    }

    int level_count = 1;

    if (texture.sampler.use_mipmap)
    {
        level_count =
            1 + floor(std::log2(std::max(texture.width, texture.height)));
        glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }

    glTextureParameteri(id, GL_TEXTURE_MAX_LEVEL, level_count);
    glTextureParameteri(id, GL_TEXTURE_BASE_LEVEL, 0);

    glTextureStorage2D(id, level_count, internal_format, texture.width,
                       texture.height);
    glTextureSubImage2D(id, 0, 0, 0, texture.width, texture.height, format,
                        GL_UNSIGNED_BYTE, texture.data.get());

    glGenerateTextureMipmap(id);

    return id;
}

size_t Renderer::register_mesh(const Mesh &mesh)
{
    unsigned int id;
    glCreateBuffers(1, &id);

    int primitive_count = mesh.indices.size();

    int size_indices = mesh.indices.size() * sizeof(uint32_t);
    int size_positions = mesh.positions.size() * sizeof(vec3);
    int size_normals = mesh.normals.size() * sizeof(vec3);
    int size_tex_coords = mesh.tex_coords.size() * sizeof(vec2);
    int size_tangents = mesh.tangents.size() * sizeof(vec4);

    // TODO: Investigate if interleaved storage is more efficient than
    // sequential storage for vertex data.
    glNamedBufferStorage(id,
                         size_indices + size_positions + size_normals +
                             size_tex_coords + size_tangents,
                         nullptr, GL_DYNAMIC_STORAGE_BIT);

    MeshInstance instance{.buffer_id = id, .primitive_count = primitive_count};

    int offset = 0;
    glNamedBufferSubData(id, offset, size_indices, mesh.indices.data());
    offset += size_indices;

    glNamedBufferSubData(id, offset, size_positions, mesh.positions.data());
    instance.positions_offset = offset;
    offset += size_positions;

    glNamedBufferSubData(id, offset, size_normals, mesh.normals.data());
    instance.normals_offset = offset;
    offset += size_normals;

    glNamedBufferSubData(id, offset, size_tex_coords, mesh.tex_coords.data());
    instance.tex_coords_offset = offset;
    offset += size_tex_coords;

    glNamedBufferSubData(id, offset, size_tangents, mesh.tangents.data());
    instance.tangents_offset = offset;

    ctx_r.mesh_instances.emplace_back(instance);
    return ctx_r.mesh_instances.size() - 1;
}

void Renderer::render_mesh_instance(unsigned int vao, const MeshInstance &m)
{
    ZoneScoped;

    glVertexArrayElementBuffer(vao, m.buffer_id);
    glVertexArrayVertexBuffer(vao, binding_positions, m.buffer_id,
                              m.positions_offset, sizeof(vec3));
    glVertexArrayVertexBuffer(vao, binding_normals, m.buffer_id,
                              m.normals_offset, sizeof(vec3));
    glVertexArrayVertexBuffer(vao, binding_tex_coords, m.buffer_id,
                              m.tex_coords_offset, sizeof(vec2));
    glVertexArrayVertexBuffer(vao, binding_tangents, m.buffer_id,
                              m.tangents_offset, sizeof(vec4));

    glDrawElements(GL_TRIANGLES, m.primitive_count, GL_UNSIGNED_INT, nullptr);
}

void Renderer::render(std::vector<RenderData> &queue)
{
    ctx_v.proj = perspective(ctx_v.fov,
                             static_cast<float>(ctx_v.size.x) /
                                 static_cast<float>(ctx_v.size.y),
                             ctx_v.near, ctx_v.far);

    ctx_v.view = camera.get_view();

    ctx_r.queue = std::move(queue);

    shadow.render(ctx_v, ctx_r);
    geometry.render(ctx_v, ctx_r);
    ssao.render(ctx_v);
    lighting.render(ctx_v, ctx_r);
    forward.render(ctx_v, ctx_r);
    bloom.render(ctx_v, ctx_r);
    tone_map.render(ctx_v, ctx_r);
}

void Renderer::resize_viewport(glm::vec2 size)
{
    ctx_v.size = size;
    // TODO: clean up buffers
}

IrradianceProbe Renderer::generate_probe(vec3 position)
{
    IrradianceProbe probe{position, invalid_texture_id};

    probe_view.ctx.proj = perspective(probe_view.ctx.fov, 1.f,
                                      probe_view.ctx.near, probe_view.ctx.far);

    array<mat4, 6> views{
        lookAt(position, position + vec3{1.f, 0.f, 0.f}, vec3{0.f, -1.f, 0.f}),
        lookAt(position, position + vec3{-1.f, 0.f, 0.f}, vec3{0.f, -1.f, 0.f}),
        lookAt(position, position + vec3{0.f, 1.f, 0.f}, vec3{0.f, 0.f, 1.f}),
        lookAt(position, position + vec3{0.f, -1.f, 0.f}, vec3{0.f, 0.f, -1.f}),
        lookAt(position, position + vec3{0.f, 0.f, 1.f}, vec3{0.f, -1.f, 0.f}),
        lookAt(position, position + vec3{0.f, 0.f, -1.f}, vec3{0.f, -1.f, 0.f}),
    };

    for (int face_idx = 0; face_idx < 6; face_idx++)
    {
        glNamedFramebufferTextureLayer(probe_view.ctx.hdr_framebuf,
                                       GL_COLOR_ATTACHMENT0,
                                       probe_view.ctx.hdr_tex, 0, face_idx);
        probe_view.ctx.view = views[face_idx];

        probe_view.shadow.render(probe_view.ctx, ctx_r);
        probe_view.geometry.render(probe_view.ctx, ctx_r);
        probe_view.lighting.render(probe_view.ctx, ctx_r);
        probe_view.forward.render(probe_view.ctx, ctx_r);
    }

    probe.cubemap = probe_view.ctx.hdr_tex;
    return probe;
}

void Renderer::generate_probe_grid_gpu(std::vector<RenderData> &queue,
                                       glm::vec3 center, glm::vec3 world_dims,
                                       float distance, int bounce_count)
{
    ctx_r.queue = std::move(queue);

    // FIXME:
    probe_view.ctx.ao_tex = ctx_v.ao_tex;

    ivec3 dims = world_dims / distance;
    int probe_count = dims.x * dims.y * dims.z;

    logger.info("Baking {} probes", probe_count);

    // We can pack the 27 coefficient into 7 4 channel textures
    const int texture_count = ctx_r.sh_texs.size();

    const ivec2 local_size{16, 16};
    const ivec3 group_count{probe_view.ctx.size / local_size, 6};

    const vec3 origin = center - world_dims / 2.f;

    const mat4 grid_transform =
        scale(translate(mat4(1.f), origin), vec3(distance));
    ctx_r.inv_grid_transform = inverse(grid_transform);

    ctx_r.grid_dims = dims;

    glDeleteTextures(texture_count, ctx_r.sh_texs.data());
    glCreateTextures(GL_TEXTURE_3D, texture_count, ctx_r.sh_texs.data());

    for (int tex_idx = 0; tex_idx < texture_count; tex_idx++)
    {
        glTextureStorage3D(ctx_r.sh_texs[tex_idx], 1, GL_RGBA16F, dims.x,
                           dims.y, dims.z);

        glTextureParameteri(ctx_r.sh_texs[tex_idx], GL_TEXTURE_WRAP_S,
                            GL_CLAMP_TO_EDGE);
        glTextureParameteri(ctx_r.sh_texs[tex_idx], GL_TEXTURE_WRAP_T,
                            GL_CLAMP_TO_EDGE);
        glTextureParameteri(ctx_r.sh_texs[tex_idx], GL_TEXTURE_WRAP_R,
                            GL_CLAMP_TO_EDGE);

        // Important for trilinear interpolation!
        glTextureParameteri(ctx_r.sh_texs[tex_idx], GL_TEXTURE_MAG_FILTER,
                            GL_LINEAR);
        glTextureParameteri(ctx_r.sh_texs[tex_idx], GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR_MIPMAP_LINEAR);
    }

    uint ssbo;
    glCreateBuffers(1, &ssbo);
    glNamedBufferStorage(ssbo,
                         group_count.x * group_count.y * group_count.z *
                             texture_count * sizeof(vec4),
                         nullptr, GL_NONE);

    // Compute the final weight for integration
    float weight_sum = 0.f;
    for (int y = 0; y < probe_view.ctx.size.x; y++)
    {
        for (int x = 0; x < probe_view.ctx.size.y; x++)
        {
            const vec2 coords = ((vec2(x, y) + vec2(0.5f)) /
                                 static_cast<vec2>(probe_view.ctx.size)) *
                                    2.0f -
                                1.0f;

            const float tmp = 1.f + coords.s * coords.s + coords.t * coords.t;
            weight_sum += 4.f / (sqrt(tmp) * tmp);
        }
    }

    weight_sum = 4.f * pi<float>() / weight_sum;

    for (int i = 0; i < bounce_count; i++)
    {
        probe_view.lighting.indirect_light = i != 0;

        for (int z = 0; z < dims.z; z++)
        {
            for (int y = 0; y < dims.y; y++)
            {
                for (int x = 0; x < dims.x; x++)
                {
                    vec3 position = vec3(grid_transform * vec4{x, y, z, 1});

                    auto probe = generate_probe(position);
                    // FIXME:
                    if (i == 0)
                        ctx_r.probes.emplace_back(probe);

                    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                                    GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                    glUseProgram(project.get_id());
                    project.set("u_weight_sum", weight_sum);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);
                    glBindImageTexture(0, probe.cubemap, 0, true, 0,
                                       GL_READ_ONLY, GL_RGBA16F);

                    glDispatchCompute(group_count.x, group_count.y,
                                      group_count.z);

                    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                    glUseProgram(reduce.get_id());
                    reduce.set("u_idx", ivec3{x, y, z});
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, ssbo);
                    for (int j = 0; j < texture_count; j++)
                        glBindImageTexture(j, ctx_r.sh_texs[j], 0, true, 0,
                                           GL_WRITE_ONLY, GL_RGBA16F);

                    glDispatchCompute(1, 1, 1);
                }
            }
        }

        logger.info("Bounce {} complete.", i);
    }

    logger.info("Probe grid generation finished");
}

enum CubeMapFace
{
    positive_x,
    negative_x,
    positive_y,
    negative_y,
    positive_z,
    negative_z,
};

// Real valued coefficients of first 3 bands of SH functions sampled at a
// given point on the unit sphere.
struct SphericalHarmonics3
{
    array<float, 9> coefficients;

    // Evaluation of the SH basis functions at the given point.
    explicit SphericalHarmonics3(const vec3 &s)
        : coefficients{
              // Band 0
              0.282095f,
              // Band 1
              0.488603f * s.y,
              0.488603f * s.z,
              0.488603f * s.x,
              // Band 2
              1.092548f * s.x * s.y,
              //   1.092548f * s.y * s.z,
              0.315392f * (3.0f * s.z * s.z - 1.0f),
              1.092548f * s.x * s.z,
              0.546274f * (s.x * s.x - s.y * s.y),
          }
    {
    }
};

// Convert a texture coordinate and cube map face to a vector directed to
// the corresponding texel in world space.
vec3 cube_map_st_to_dir(vec2 coords, CubeMapFace face)
{
    // Map [0, 1] to [-1, 1].
    coords = 2.f * coords - 1.f;

    // https://www.khronos.org/opengl/wiki/Cubemap_Texture#Upload_and_orientation
    switch (face)
    {
    case CubeMapFace::positive_x:
        return vec3{1.0f, -coords.t, -coords.s};
    case CubeMapFace::negative_x:
        return vec3{-1.0f, -coords.t, coords.s};
    case CubeMapFace::positive_y:
        return vec3{coords.s, 1.0f, coords.t};
    case CubeMapFace::negative_y:
        return vec3{coords.s, -1.0f, -coords.t};
    case CubeMapFace::positive_z:
        return vec3{coords.s, -coords.t, 1.0f};
    case CubeMapFace::negative_z:
        return vec3{coords.s, -coords.t, -1.0f};
    }
}

void sh_reconstruct(array<vec3, 9> sh, int size)
{
    const int face_size = size * size;
    const int face_count = 6;

    vector<glm::vec3> texels(face_size * face_count);

    int face_idx = 0;

    for (int face_idx = 0; face_idx < face_count; face_idx++)
    {
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                vec2 coords =
                    (vec2(x, y) + vec2(0.5f)) / static_cast<float>(size);
                // Project cubemap texel position on a sphere.
                vec3 s = normalize(
                    cube_map_st_to_dir(coords, (CubeMapFace)face_idx));

                // clang-format off
                texels.at(x + y * size + face_idx * face_size) =
                    sh[0] * 0.282095f +
                    sh[1] * 0.488603f * s.y +
                    sh[2] * 0.488603f * s.z +
                    sh[3] * 0.488603f * s.x;
                    sh[4] * 1.092548f * s.x * s.y +
                    sh[5] * 1.092548f * s.y * s.z +
                    sh[6] * 0.315392f * (3.0f * s.z * s.z - 1.0f) +
                    sh[7] * 1.092548f * s.x * s.z +
                    sh[8] * 0.546274f * (s.x * s.x - s.y * s.y);
                // clang-format on
            }
        }
    }

    stbi_write_hdr("radiance.hdr", size, face_count * size, 3,
                   reinterpret_cast<float *>(texels.data()));
}

array<vec3, 9> sh_project(uint cube_map, int size)
{
    const int face_size = size * size;
    const int face_count = 6;

    vector<glm::vec3> texels(face_count * face_size);
    glGetTextureImage(cube_map, 0, GL_RGB, GL_FLOAT,
                      texels.size() * sizeof(vec3), texels.data());

    array<vec3, 9> rgb_coeffs{};
    float weight_sum = 0;

    for (int face_idx = 0; face_idx < face_count; face_idx++)
    {
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                const vec3 &texel =
                    texels.at(x + y * size + face_idx * face_size);
                vec2 coords =
                    (vec2{x, y} + vec2(0.5f)) / static_cast<float>(size);
                // Project cubemap texel position on a sphere.
                vec3 s = normalize(
                    cube_map_st_to_dir(coords, (CubeMapFace)face_idx));

                // Differential solid angle.
                coords.s = coords.s * 2.f - 1.f;
                coords.t = coords.t * 2.f - 1.f;
                float tmp = 1 + coords.s * coords.s + coords.t * coords.t;
                float weight = 4.f / (sqrt(tmp) * tmp);

                // Evaluate the SH functions at center of the current texel.
                SphericalHarmonics3 sh(s);

                for (int i = 0; i < 9; i++)
                    rgb_coeffs[i] += weight * texel * sh.coefficients[i];

                weight_sum += weight;
            }
        }
    }

    for (auto &c : rgb_coeffs)
        c *= 4.f * pi<float>() / weight_sum;

    return rgb_coeffs;
}

// void Renderer::generate_probe_grid_cpu(std::vector<RenderData> &queue,
//                                        glm::vec3 center, glm::vec3
//                                        world_dims, float distance)
// {
//     logger.info("Generating probe grid");

//     ivec3 dims = world_dims / distance;
//     int probe_count = dims.x * dims.y * dims.z;
//     // We can pack the 27 coefficient into 7 RGBA textures
//     int texture_count = 7;

//     vec3 origin = center - world_dims / 2.f;

//     mat4 grid_transform = scale(translate(mat4(1.f), origin),
//     vec3(distance));

//     ctx.grid_dims = dims;
//     ctx.inv_grid_transform = inverse(grid_transform);

//     logger.debug("{}",
//                  glm::to_string(ctx.inv_grid_transform * vec4(origin, 1.f)));

//     logger.debug("{}", glm::to_string(dims));

//     // Contigious storage for 3D textures, each containing the per channel
//     // coefficients of a single SH band.
//     vector<vec4> coeffs(probe_count * texture_count);

//     ctx.sh_texs.resize(texture_count);
//     glCreateTextures(GL_TEXTURE_3D, texture_count, ctx.sh_texs.data());

//     int bounce_count = 1;

//     for (int i = 0; i < bounce_count; i++)
//     {
//         for (int z = 0; z < dims.z; z++)
//         {
//             for (int y = 0; y < dims.y; y++)
//             {
//                 for (int x = 0; x < dims.x; x++)
//                 {
//                     vec3 position = vec3(grid_transform * vec4{x, y, z, 1});

//                     // Rasterize probe environment map.
//                     auto probe = generate_probe(position, queue, i != 0);
//                     if (i == 0)
//                         probes.emplace_back(probe);

//                     // Project environment cube map on the SH basis.
//                     auto cs = sh_project(probe_env_map,
//                     probe_env_map_size.x);

//                     int idx = x + y * dims.x + z * dims.x * dims.y;

//                     coeffs.at(idx) = vec4(cs[0], cs[7][0]);
//                     coeffs.at(idx + 1 * probe_count) = vec4(cs[1], cs[7][1]);
//                     coeffs.at(idx + 2 * probe_count) = vec4(cs[2], cs[7][2]);
//                     coeffs.at(idx + 3 * probe_count) = vec4(cs[3], cs[8][0]);
//                     coeffs.at(idx + 4 * probe_count) = vec4(cs[4], cs[8][1]);
//                     coeffs.at(idx + 5 * probe_count) = vec4(cs[5], cs[8][2]);
//                     coeffs.at(idx + 6 * probe_count) = vec4(cs[6], 0);
//                 }
//             }
//         }

//         for (int tex_idx = 0; tex_idx < texture_count; tex_idx++)
//         {
//             if (i == 0)
//             {
//                 glTextureStorage3D(ctx.sh_texs[tex_idx], 1, GL_RGBA16F,
//                 dims.x,
//                                    dims.y, dims.z);
//             }

//             glTextureParameteri(ctx.sh_texs[tex_idx], GL_TEXTURE_WRAP_S,
//                                 GL_CLAMP_TO_EDGE);
//             glTextureParameteri(ctx.sh_texs[tex_idx], GL_TEXTURE_WRAP_T,
//                                 GL_CLAMP_TO_EDGE);
//             glTextureParameteri(ctx.sh_texs[tex_idx], GL_TEXTURE_WRAP_R,
//                                 GL_CLAMP_TO_EDGE);

//             // Important for trilinear interpolation!
//             glTextureParameteri(ctx.sh_texs[tex_idx], GL_TEXTURE_MAG_FILTER,
//                                 GL_LINEAR);
//             glTextureParameteri(ctx.sh_texs[tex_idx], GL_TEXTURE_MIN_FILTER,
//                                 GL_LINEAR_MIPMAP_LINEAR);

//             glTextureSubImage3D(ctx.sh_texs[tex_idx], 0, 0, 0, 0, dims.x,
//                                 dims.y, dims.z, GL_RGBA, GL_FLOAT,
//                                 &coeffs.at(tex_idx * probe_count));
//         }
//     }

//     logger.info("Probe grid generation finished");
// }