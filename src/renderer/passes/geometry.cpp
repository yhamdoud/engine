#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include "geometry.hpp"
#include "logger.hpp"
#include "model.hpp"
#include "renderer/renderer.hpp"

using namespace std;
using namespace glm;
using namespace engine;

void GeometryPass::create_debug_views()
{
    array<int, 4> rgb_swizzle{GL_RED, GL_GREEN, GL_BLUE, GL_ONE};
    array<int, 4> www_swizzle{GL_ALPHA, GL_ALPHA, GL_ALPHA, GL_ONE};

    glGenTextures(1, &debug_view_normal);
    glTextureView(debug_view_normal, GL_TEXTURE_2D, normal_metal, GL_RGBA16F, 0,
                  1, 0, 1);
    glTextureParameteriv(debug_view_normal, GL_TEXTURE_SWIZZLE_RGBA,
                         rgb_swizzle.data());

    glGenTextures(1, &debug_view_metallic);
    glTextureView(debug_view_metallic, GL_TEXTURE_2D, normal_metal, GL_RGBA16F,
                  0, 1, 0, 1);
    glTextureParameteriv(debug_view_metallic, GL_TEXTURE_SWIZZLE_RGBA,
                         www_swizzle.data());

    glGenTextures(1, &debug_view_base_color);
    glTextureView(debug_view_base_color, GL_TEXTURE_2D, base_color_rough,
                  GL_RGBA8, 0, 1, 0, 1);
    glTextureParameteriv(debug_view_base_color, GL_TEXTURE_SWIZZLE_RGBA,
                         rgb_swizzle.data());

    glGenTextures(1, &debug_view_roughness);
    glTextureView(debug_view_roughness, GL_TEXTURE_2D, base_color_rough,
                  GL_RGBA8, 0, 1, 0, 1);
    glTextureParameteriv(debug_view_roughness, GL_TEXTURE_SWIZZLE_RGBA,
                         www_swizzle.data());
}

GeometryPass::GeometryPass()
{
    glCreateFramebuffers(1, &f_buf);
    glCreateTextures(GL_TEXTURE_2D, 1, &normal_metal);
    glCreateTextures(GL_TEXTURE_2D, 1, &base_color_rough);
    glCreateTextures(GL_TEXTURE_2D, 1, &depth);
}

void GeometryPass::initialize(ViewportContext &ctx)
{

    glTextureStorage2D(normal_metal, 1, GL_RGBA16F, ctx.size.x, ctx.size.y);
    glTextureParameteri(normal_metal, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(normal_metal, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTextureStorage2D(base_color_rough, 1, GL_RGBA16F, ctx.size.x, ctx.size.y);
    glTextureParameteri(base_color_rough, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(base_color_rough, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTextureStorage2D(depth, 1, GL_DEPTH_COMPONENT24, ctx.size.x, ctx.size.y);
    glTextureParameteri(depth, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(depth, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(depth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(depth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glNamedFramebufferTexture(f_buf, GL_COLOR_ATTACHMENT1, normal_metal, 0);
    glNamedFramebufferTexture(f_buf, GL_COLOR_ATTACHMENT2, base_color_rough, 0);
    glNamedFramebufferTexture(f_buf, GL_DEPTH_ATTACHMENT, depth, 0);

    array<GLenum, 3> draw_bufs{
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
    };
    glNamedFramebufferDrawBuffers(f_buf, 3, draw_bufs.data());

    if (glCheckNamedFramebufferStatus(f_buf, GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE)
        logger.error("G-buffer incomplete");

    ctx.g_buf = GBuffer{ctx.size, f_buf, base_color_rough, normal_metal, depth};

    create_debug_views();
}

void GeometryPass::render(ViewportContext &ctx_v, RenderContext &ctx_r)
{
    glViewport(0, 0, ctx_v.size.x, ctx_v.size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx_v.g_buf.framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(ctx_r.entity_vao);

    uint cur_shader_id = invalid_shader_id;

    for (auto &r : ctx_r.queue)
    {
        if (auto shader_id = r.shader.get_id(); cur_shader_id != shader_id)
        {
            glUseProgram(shader_id);
            cur_shader_id = shader_id;
        }

        const auto model_view = ctx_v.view * r.model;
        const auto mvp = ctx_v.proj * model_view;

        r.shader.set("u_model_view", model_view);
        r.shader.set("u_mvp", mvp);
        r.shader.set("u_normal_mat", inverseTranspose(mat3{model_view}));
        float far_clip_dist = ctx_v.proj[3][2] / (ctx_v.proj[2][2] + 1.f);
        r.shader.set("u_far_clip_distance", far_clip_dist);

        r.shader.set("u_base_color_factor", r.base_color_factor);
        r.shader.set("u_metallic_factor", r.metallic_factor);
        r.shader.set("u_roughness_factor", r.roughness_factor);
        r.shader.set("u_alpha_mask", r.alpha_mode == AlphaMode::mask);
        r.shader.set("u_alpha_cutoff", r.alpha_cutoff);

        if (r.base_color_tex_id != invalid_texture_id)
        {
            r.shader.set("u_use_sampler", true);
            r.shader.set("u_base_color", 0);
            glBindTextureUnit(0, r.base_color_tex_id);
        }
        else
        {
            r.shader.set("u_use_sampler", false);
        }

        if (r.normal_tex_id != invalid_texture_id)
        {
            r.shader.set("u_use_normal", true);
            r.shader.set("u_normal", 1);
            glBindTextureUnit(1, r.normal_tex_id);
        }
        else
        {
            r.shader.set("u_use_normal", false);
        }

        if (r.metallic_roughness_id != invalid_texture_id)
        {
            r.shader.set("u_use_metallic_roughness", true);
            r.shader.set("u_metallic_roughness", 2);
            glBindTextureUnit(2, r.metallic_roughness_id);
        }
        else
        {
            r.shader.set("u_use_metallic_roughness", false);
        }

        Renderer::render_mesh_instance(ctx_r.entity_vao,
                                       ctx_r.mesh_instances[r.mesh_index]);
    }
}