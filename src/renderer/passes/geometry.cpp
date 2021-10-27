#include <Tracy.hpp>
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
    array<int, 4> rg_swizzle{GL_RED, GL_GREEN, GL_ZERO, GL_ONE};
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
                  GL_SRGB8_ALPHA8, 0, 1, 0, 1);
    glTextureParameteriv(debug_view_base_color, GL_TEXTURE_SWIZZLE_RGBA,
                         rgb_swizzle.data());

    glGenTextures(1, &debug_view_roughness);
    glTextureView(debug_view_roughness, GL_TEXTURE_2D, base_color_rough,
                  GL_SRGB8_ALPHA8, 0, 1, 0, 1);
    glTextureParameteriv(debug_view_roughness, GL_TEXTURE_SWIZZLE_RGBA,
                         www_swizzle.data());

    glGenTextures(1, &debug_view_velocity);
    glTextureView(debug_view_velocity, GL_TEXTURE_2D, velocity, GL_RGBA16F, 0,
                  1, 0, 1);
    glTextureParameteriv(debug_view_velocity, GL_TEXTURE_SWIZZLE_RGBA,
                         rg_swizzle.data());
}

GeometryPass::GeometryPass()
{
    glCreateFramebuffers(2, &fbuf);
    downsample_shader.set("u_read", 3);
}

void GeometryPass::initialize(ViewportContext &ctx)
{
    glDeleteTextures(4, &normal_metal);
    glCreateTextures(GL_TEXTURE_2D, 4, &normal_metal);

    glTextureStorage2D(normal_metal, 1, GL_RGBA16F, ctx.size.x, ctx.size.y);

    glTextureStorage2D(base_color_rough, 1, GL_SRGB8_ALPHA8, ctx.size.x,
                       ctx.size.y);

    glTextureStorage2D(velocity, 1, GL_RGBA16F, ctx.size.x, ctx.size.y);

    glTextureStorage2D(depth, 2, GL_DEPTH_COMPONENT32, ctx.size.x, ctx.size.y);
    glTextureParameteri(depth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(depth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(depth, GL_TEXTURE_MIN_FILTER,
                        GL_NEAREST_MIPMAP_NEAREST);
    glTextureParameteri(depth, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glNamedFramebufferTexture(fbuf, GL_DEPTH_ATTACHMENT, depth, 0);
    glNamedFramebufferTexture(fbuf, GL_COLOR_ATTACHMENT0, normal_metal, 0);
    glNamedFramebufferTexture(fbuf, GL_COLOR_ATTACHMENT1, base_color_rough, 0);
    glNamedFramebufferTexture(fbuf, GL_COLOR_ATTACHMENT2, velocity, 0);

    array<GLenum, 3> draw_bufs{
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
    };
    glNamedFramebufferDrawBuffers(fbuf, draw_bufs.size(), draw_bufs.data());

    if (glCheckNamedFramebufferStatus(fbuf, GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE)
        logger.error("G-buffer incomplete");

    ctx.g_buf = GBuffer{ctx.size,     fbuf,     base_color_rough,
                        normal_metal, velocity, depth};

    create_debug_views();

    glNamedFramebufferTexture(fbuf_downsample, GL_DEPTH_ATTACHMENT, depth, 1);
    glNamedFramebufferDrawBuffer(fbuf_downsample, GL_NONE);
    glNamedFramebufferReadBuffer(fbuf_downsample, GL_NONE);
    if (glCheckNamedFramebufferStatus(fbuf_downsample, GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE)
        logger.error("Downsample framebuffer incomplete");
}

void GeometryPass::render(ViewportContext &ctx_v, RenderContext &ctx_r)
{
    ZoneScoped;

    glViewport(0, 0, ctx_v.size.x, ctx_v.size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx_v.g_buf.framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(ctx_r.entity_vao);

    glUseProgram(shader.get_id());

    for (auto &r : ctx_r.queue)
    {
        const auto model_view = ctx_v.view * r.model;
        const auto mvp = ctx_v.proj * model_view;
        const auto mvp_prev = ctx_v.view_proj_prev * r.model;

        shader.set("u_mvp", mvp);
        shader.set("u_mvp_prev", mvp_prev);
        shader.set("u_normal_mat", inverseTranspose(mat3{model_view}));

        shader.set("u_base_color_factor", vec3(r.material.base_color_factor));
        shader.set("u_metallic_factor", r.material.metallic_factor);
        shader.set("u_roughness_factor", r.material.roughness_factor);
        shader.set("u_alpha_mask", r.material.alpha_mode == AlphaMode::mask);
        shader.set("u_alpha_cutoff", r.material.alpha_cutoff);

        if (r.material.base_color != invalid_texture_id)
        {
            shader.set("u_use_sampler", true);
            shader.set("u_base_color", 0);
            glBindTextureUnit(0, r.material.base_color);
        }
        else
        {
            shader.set("u_use_sampler", false);
        }

        if (r.material.normal != invalid_texture_id)
        {
            shader.set("u_use_normal", true);
            shader.set("u_normal", 1);
            glBindTextureUnit(1, r.material.normal);
        }
        else
        {
            shader.set("u_use_normal", false);
        }

        if (r.material.metallic_roughness != invalid_texture_id)
        {
            shader.set("u_use_metallic_roughness", true);
            shader.set("u_metallic_roughness", 2);
            glBindTextureUnit(2, r.material.metallic_roughness);
        }
        else
        {
            shader.set("u_use_metallic_roughness", false);
        }

        Renderer::render_mesh_instance(ctx_r.mesh_instances[r.mesh_index]);
    }

    {
        glViewport(0, 0, ctx_v.size.x / 2, ctx_v.size.y / 2);
        glBindFramebuffer(GL_FRAMEBUFFER, fbuf_downsample);
        glClear(GL_DEPTH_BUFFER_BIT);
        glBindTextureUnit(3, depth);

        glUseProgram(downsample_shader.get_id());
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
}