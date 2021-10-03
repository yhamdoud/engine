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
#include "profiler.hpp"
#include "renderer.hpp"

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

Renderer::Renderer(glm::ivec2 viewport_size)
{
    ctx_v.size = viewport_size;

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

        glCreateFramebuffers(1, &ctx_v.hdr_frame_buf);
        glNamedFramebufferTexture(ctx_v.hdr_frame_buf, GL_COLOR_ATTACHMENT0,
                                  ctx_v.hdr_tex, 0);
        glNamedFramebufferRenderbuffer(ctx_v.hdr_frame_buf, GL_DEPTH_ATTACHMENT,
                                       GL_RENDERBUFFER, depth);

        if (glCheckNamedFramebufferStatus(
                ctx_v.hdr_frame_buf, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            logger.error("Main viewport frame buffer incomplete.");
    }

    // Setup state for displaying probe.
    auto sphere_model =
        move(get<vector<Model>>(load_gltf(models_path / "sphere.glb"))[0]);
    ctx_r.probe_mesh_idx = register_mesh(*sphere_model.mesh);

    shadow.initialize(ctx_v);
    geometry.initialize(ctx_v);
    ssao.initialize(ctx_v);
    lighting.initialize(ctx_v);
    forward.initialize(ctx_v);
    ssr.initialize(ctx_v);
    bloom.initialize(ctx_v);
    tone_map.initialize(ctx_v);
}

Renderer::~Renderer()
{
    // TODO: clean up mesh instances.
}

// Source: GLI manual.
variant<uint, Renderer::Error>
Renderer::register_texture(const CompressedTexture &tex)
{
    gli::gl gl(gli::gl::PROFILE_GL33);
    const gli::gl::format fmt = gl.translate(tex.format(), tex.swizzles());
    GLenum target = gl.translate(tex.target());

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(target, id);
    glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(target, GL_TEXTURE_MAX_LEVEL,
                    static_cast<GLint>(tex.levels() - 1));
    glTexParameteri(target, GL_TEXTURE_SWIZZLE_R, fmt.Swizzles[0]);
    glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, fmt.Swizzles[1]);
    glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, fmt.Swizzles[2]);
    glTexParameteri(target, GL_TEXTURE_SWIZZLE_A, fmt.Swizzles[3]);

    const tvec3<GLsizei> extent(tex.extent());
    const GLsizei face_count = static_cast<GLsizei>(tex.layers() * tex.faces());

    switch (tex.target())
    {
    case gli::TARGET_1D:
        glTexStorage1D(target, static_cast<GLint>(tex.levels()), fmt.Internal,
                       extent.x);
        break;
    case gli::TARGET_1D_ARRAY:
    case gli::TARGET_2D:
    case gli::TARGET_CUBE:
        glTexStorage2D(target, static_cast<GLint>(tex.levels()), fmt.Internal,
                       extent.x,
                       tex.target() == gli::TARGET_2D ? extent.y : face_count);
        break;
    case gli::TARGET_2D_ARRAY:
    case gli::TARGET_3D:
    case gli::TARGET_CUBE_ARRAY:
        glTexStorage3D(target, static_cast<GLint>(tex.levels()), fmt.Internal,
                       extent.x, extent.y,
                       tex.target() == gli::TARGET_3D ? extent.z : face_count);
        break;
    default:
        assert(0);
        break;
    }

    for (std::size_t layer = 0; layer < tex.layers(); ++layer)
        for (std::size_t face = 0; face < tex.faces(); ++face)
            for (std::size_t level = 0; level < tex.levels(); ++level)
            {
                GLsizei const layer_gl = static_cast<GLsizei>(layer);
                glm::tvec3<GLsizei> extent(tex.extent(level));
                target = gli::is_target_cube(tex.target())
                             ? static_cast<GLenum>(
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + face)
                             : target;

                switch (tex.target())
                {
                case gli::TARGET_1D:
                    if (gli::is_compressed(tex.format()))
                        glCompressedTexSubImage1D(
                            target, static_cast<GLint>(level), 0, extent.x,
                            fmt.Internal, static_cast<GLsizei>(tex.size(level)),
                            tex.data(layer, face, level));
                    else
                        glTexSubImage1D(target, static_cast<GLint>(level), 0,
                                        extent.x, fmt.External, fmt.Type,
                                        tex.data(layer, face, level));
                    break;
                case gli::TARGET_1D_ARRAY:
                case gli::TARGET_2D:
                case gli::TARGET_CUBE:
                    if (gli::is_compressed(tex.format()))
                        glCompressedTexSubImage2D(
                            target, static_cast<GLint>(level), 0, 0, extent.x,
                            tex.target() == gli::TARGET_1D_ARRAY ? layer_gl
                                                                 : extent.y,
                            fmt.Internal, static_cast<GLsizei>(tex.size(level)),
                            tex.data(layer, face, level));
                    else
                        glTexSubImage2D(
                            target, static_cast<GLint>(level), 0, 0, extent.x,
                            tex.target() == gli::TARGET_1D_ARRAY ? layer_gl
                                                                 : extent.y,
                            fmt.External, fmt.Type,
                            tex.data(layer, face, level));
                    break;
                case gli::TARGET_2D_ARRAY:
                case gli::TARGET_3D:
                case gli::TARGET_CUBE_ARRAY:
                    if (gli::is_compressed(tex.format()))
                        glCompressedTexSubImage3D(
                            target, static_cast<GLint>(level), 0, 0, 0,
                            extent.x, extent.y,
                            tex.target() == gli::TARGET_3D ? extent.z
                                                           : layer_gl,
                            fmt.Internal, static_cast<GLsizei>(tex.size(level)),
                            tex.data(layer, face, level));
                    else
                        glTexSubImage3D(target, static_cast<GLint>(level), 0, 0,
                                        0, extent.x, extent.y,
                                        tex.target() == gli::TARGET_3D
                                            ? extent.z
                                            : layer_gl,
                                        fmt.External, fmt.Type,
                                        tex.data(layer, face, level));
                    break;
                default:
                    assert(0);
                    break;
                }
            }

    return id;
}

variant<uint, Renderer::Error> Renderer::register_texture(const Texture &tex)
{
    uint id;
    glCreateTextures(GL_TEXTURE_2D, 1, &id);

    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, tex.sampler.magnify_filter);
    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, tex.sampler.minify_filter);
    glTextureParameteri(id, GL_TEXTURE_WRAP_S, tex.sampler.wrap_s);
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, tex.sampler.wrap_t);

    GLenum internal_format, format;

    switch (tex.component_count)
    {
    case 3:
        internal_format = (tex.sampler.is_srgb) ? GL_SRGB8 : GL_RGB8;
        format = GL_RGB;
        break;
    case 4:
        internal_format = (tex.sampler.is_srgb) ? GL_SRGB8_ALPHA8 : GL_RGB8;
        format = GL_RGBA;
        break;
    default:
        return Renderer::Error::unsupported_texture_format;
    }

    int level_count = 1;

    if (tex.sampler.use_mipmap)
    {
        level_count = 1 + floor(std::log2(std::max(tex.width, tex.height)));
        glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }

    glTextureParameteri(id, GL_TEXTURE_MAX_LEVEL, level_count);
    glTextureParameteri(id, GL_TEXTURE_BASE_LEVEL, 0);

    glTextureStorage2D(id, level_count, internal_format, tex.width, tex.height);
    glTextureSubImage2D(id, 0, 0, 0, tex.width, tex.height, format,
                        GL_UNSIGNED_BYTE, tex.data.get());

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
    ZoneScoped;

    ctx_v.proj = perspective(ctx_v.fov,
                             static_cast<float>(ctx_v.size.x) /
                                 static_cast<float>(ctx_v.size.y),
                             ctx_v.near, ctx_v.far);
    ctx_v.proj_inv = inverse(ctx_v.proj);

    ctx_v.view = camera.get_view();

    ctx_r.queue = std::move(queue);

    if (baking_jobs.size() > 0)
    {
        TracyGpuZone("Probe baking pass") bake();
    }

    {
        TracyGpuZone("Shadow pass") shadow.render(ctx_v, ctx_r);
    }

    {
        TracyGpuZone("Geometry pass") geometry.render(ctx_v, ctx_r);
    }

    if (ssao_enabled)
    {
        TracyGpuZone("SSAO pass") ssao.render(ctx_v);
    }

    {
        TracyGpuZone("Lighting pass") lighting.render(ctx_v, ctx_r);
    }

    {
        TracyGpuZone("Forward pass") forward.render(ctx_v, ctx_r);
    }

    if (ssr_enabled)
    {
        TracyGpuZone("SSR pass") ssr.render(ctx_v, ctx_r);
    }

    if (bloom_enabled)
    {
        TracyGpuZone("Bloom pass") bloom.render(ctx_v, ctx_r);
    }

    {
        TracyGpuZone("Tone map pass") tone_map.render(ctx_v, ctx_r);
    }
}

void Renderer::resize_viewport(glm::vec2 size)
{
    ctx_v.size = size;
    // TODO: clean up buffers
}

void Renderer::prepare_bake(glm::vec3 center, glm::vec3 world_dims,
                            float distance, int bounce_count)
{

    // We pack the 27 coefficient into 7 4 channel textures
    const vec3 origin = center - world_dims / 2.f;
    const ivec2 local_size{16, 16};

    probe_grid = ProbeGrid{
        .bounce_count = bounce_count,
        .dims = world_dims / distance,
        .group_count = ivec3(probe_view.ctx.size / local_size, 6),
        .grid_transform = scale(translate(mat4(1.f), origin), vec3(distance)),
        .weight_sum = 0.f,
    };

    if (probe_grid.dims != probe_buf.get_size())
        probe_buf.resize(probe_grid.dims);

    glCreateBuffers(1, &probe_grid.coef_buf);
    glNamedBufferStorage(probe_grid.coef_buf,
                         probe_grid.group_count.x * probe_grid.group_count.y *
                             probe_grid.group_count.z * 7 * sizeof(vec4),
                         nullptr, GL_NONE);

    for (int z = 0; z < probe_grid.dims.z; z++)
        for (int y = 0; y < probe_grid.dims.y; y++)
            for (int x = 0; x < probe_grid.dims.x; x++)
                baking_jobs.emplace_back(BakingJob{ivec3(x, y, z)});

    // Compute the final weight for integration
    for (int y = 0; y < probe_view.ctx.size.x; y++)
    {
        for (int x = 0; x < probe_view.ctx.size.y; x++)
        {
            const vec2 coords = ((vec2(x, y) + vec2(0.5f)) /
                                 static_cast<vec2>(probe_view.ctx.size)) *
                                    2.0f -
                                1.0f;

            const float tmp = 1.f + coords.s * coords.s + coords.t * coords.t;
            probe_grid.weight_sum += 4.f / (sqrt(tmp) * tmp);
        }
    }

    probe_grid.weight_sum = 4.f * pi<float>() / probe_grid.weight_sum;

    // Update context.
    ctx_r.inv_grid_transform = inverse(probe_grid.grid_transform);
    ctx_r.grid_dims = probe_grid.dims;

    cur_bounce_idx = 0;
    ctx_r.probes.clear();

    logger.info("Baking {} probes, {} bounce(s).",
                probe_grid.dims.x * probe_grid.dims.y * probe_grid.dims.z,
                probe_grid.bounce_count);
}

void Renderer::bake()
{
    ZoneScoped;

    int stop_idx = cur_baking_offset + bake_batch_size;
    bool final_batch = false;

    if (stop_idx >= baking_jobs.size())
    {
        stop_idx = baking_jobs.size() - 1;
        final_batch = true;
    }

    for (int idx = cur_baking_offset; idx < stop_idx; idx++)
    {
        probe_view.lighting.indirect_light = cur_bounce_idx != 0;
        probe_view.position = vec3(probe_grid.grid_transform *
                                   vec4{baking_jobs[idx].coords, 1.f});
        probe_view.render(ctx_r);
        probe_view.bake(ctx_r, baking_jobs[idx], cur_bounce_idx, probe_grid,
                        probe_buf);
    }

    cur_baking_offset = stop_idx;

    // Schedule new jobs if there are any bounces left.
    if (final_batch)
    {
        logger.info("Bounce {}/{} baked.", cur_bounce_idx + 1,
                    probe_grid.bounce_count);

        cur_baking_offset = 0;

        probe_buf.swap();
        ctx_r.sh_texs = span<uint, 7>(probe_buf.front(), ProbeBuffer::count);

        if (cur_bounce_idx == probe_grid.bounce_count - 1)
            baking_jobs.clear();
        else
            cur_bounce_idx++;
    }
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

float Renderer::baking_progress()
{
    return is_baking() *
           (static_cast<float>(cur_bounce_idx) +
            static_cast<float>(cur_baking_offset + 1) /
                static_cast<float>(baking_jobs.size())) /
           static_cast<float>(probe_grid.bounce_count);
}

bool Renderer::is_baking() { return baking_jobs.size() != 0; }

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