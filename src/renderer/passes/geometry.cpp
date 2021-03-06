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
    glTextureView(debug_view_velocity, GL_TEXTURE_2D, velocity, GL_RGBA8_SNORM,
                  0, 1, 0, 1);
    glTextureParameteriv(debug_view_velocity, GL_TEXTURE_SWIZZLE_RGBA,
                         rg_swizzle.data());
}

GeometryPass::GeometryPass()
{
    glCreateFramebuffers(2, &fbuf);

    array<GLenum, 4> draw_bufs{
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
    };
    glNamedFramebufferDrawBuffers(fbuf, draw_bufs.size(), draw_bufs.data());

    glNamedFramebufferDrawBuffer(fbuf_downsample, GL_NONE);
    glNamedFramebufferReadBuffer(fbuf_downsample, GL_NONE);

    downsample_shader.set("u_read", 3);
}

void GeometryPass::initialize(ViewportContext &ctx)
{
    glDeleteTextures(4, &normal_metal);
    glCreateTextures(GL_TEXTURE_2D, 4, &normal_metal);

    glDeleteTextures(1, &ctx.id_tex);
    glCreateTextures(GL_TEXTURE_2D, 1, &ctx.id_tex);

    glTextureStorage2D(normal_metal, 1, GL_RGBA16F, ctx.size.x, ctx.size.y);

    glTextureStorage2D(base_color_rough, 1, GL_SRGB8_ALPHA8, ctx.size.x,
                       ctx.size.y);

    glTextureStorage2D(velocity, 1, GL_RGBA16F, ctx.size.x, ctx.size.y);

    glTextureStorage2D(depth, 2, GL_DEPTH_COMPONENT32, ctx.size.x, ctx.size.y);
    glTextureParameteri(depth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(depth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(depth, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(depth, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTextureStorage2D(ctx.id_tex, 1, GL_R32UI, ctx.size.x, ctx.size.y);

    glNamedFramebufferTexture(fbuf, GL_DEPTH_ATTACHMENT, depth, 0);
    glNamedFramebufferTexture(fbuf, GL_COLOR_ATTACHMENT0, normal_metal, 0);
    glNamedFramebufferTexture(fbuf, GL_COLOR_ATTACHMENT1, base_color_rough, 0);
    glNamedFramebufferTexture(fbuf, GL_COLOR_ATTACHMENT2, velocity, 0);
    glNamedFramebufferTexture(fbuf, GL_COLOR_ATTACHMENT3, ctx.id_tex, 0);

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

void GeometryPass::render(const RenderArgs &args)
{
    ZoneScoped;

    glViewport(0, 0, args.size.x, args.size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, args.framebuf);

    const uvec4 zero(0u);
    const uvec4 minus_one(-1);
    glClearNamedFramebufferuiv(fbuf, GL_COLOR, 0, value_ptr(zero));
    glClearNamedFramebufferuiv(fbuf, GL_COLOR, 1, value_ptr(zero));
    glClearNamedFramebufferuiv(fbuf, GL_COLOR, 2, value_ptr(zero));
    glClearNamedFramebufferuiv(fbuf, GL_COLOR, 3, value_ptr(minus_one));
    glClear(GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(args.entity_vao);

    glUseProgram(shader.get_id());

    for (auto &r : args.entities)
    {
        const auto model_view = args.view * r.model;
        const auto mvp = args.view_proj * r.model;
        const auto mvp_prev = args.view_proj_prev * r.model;

        shader.set("u_mvp", mvp);
        shader.set("u_mvp_prev", mvp_prev);
        shader.set("u_normal_mat", inverseTranspose(mat3{model_view}));

        shader.set("u_base_color_factor", vec3(r.material.base_color_factor));
        shader.set("u_metallic_factor", r.material.metallic_factor);
        shader.set("u_roughness_factor", r.material.roughness_factor);
        shader.set("u_alpha_mask", r.material.alpha_mode == AlphaMode::mask);
        shader.set("u_alpha_cutoff", r.material.alpha_cutoff);

        shader.set("u_jitter", args.jitter);
        shader.set("u_jitter_prev", args.jitter_prev);

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

        Renderer::render_mesh_instance(args.meshes[r.mesh_index]);
    }

    glUseProgram(point_light_shader.get_id());
    glDepthMask(false);

    point_light_shader.set("u_view_proj", args.view_proj);

    for (uint32_t i = 0; i < args.lights.size(); i++)
    {
        const auto &light = args.lights[i];

        point_light_shader.set("u_light.position", light.position);
        point_light_shader.set("u_light.radius", 0.5f);
        point_light_shader.set("u_id", i);

        Renderer::render_mesh_instance(args.sphere_mesh);
    }

    glDepthMask(true);

    {
        glViewport(0, 0, args.size.x / 2, args.size.y / 2);
        glBindFramebuffer(GL_FRAMEBUFFER, fbuf_downsample);
        glClear(GL_DEPTH_BUFFER_BIT);
        glBindTextureUnit(3, depth);

        glUseProgram(downsample_shader.get_id());
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
}