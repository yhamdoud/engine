#include "logger.hpp"
#include "profiler.hpp"
#include "renderer/context.hpp"
#include "renderer/renderer.hpp"

using namespace engine;
using namespace glm;
using namespace std;

ProbeViewport::ProbeViewport()
{
    uint depth;
    glCreateRenderbuffers(1, &depth);
    glNamedRenderbufferStorage(depth, GL_DEPTH_COMPONENT32, ctx.size.x,
                               ctx.size.y);

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &ctx.hdr_tex);
    glTextureStorage2D(ctx.hdr_tex, 1, GL_RGBA16F, ctx.size.x, ctx.size.y);
    glTextureParameteri(ctx.hdr_tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(ctx.hdr_tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateFramebuffers(1, &ctx.hdr_frame_buf);
    glNamedFramebufferRenderbuffer(ctx.hdr_frame_buf, GL_DEPTH_ATTACHMENT,
                                   GL_RENDERBUFFER, depth);

    if (glCheckNamedFramebufferStatus(ctx.hdr_frame_buf, GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE)
        logger.error("Probe frame buffer incomplete.");

    project = *Shader::from_comp_path(shaders_path / "sh_project.comp");
    reduce = *Shader::from_comp_path(shaders_path / "sh_reduce.comp");

    shadow.initialize(ctx);
    geometry.initialize(ctx);
    lighting.initialize(ctx);
    forward.initialize(ctx);
}

void ProbeViewport::render(RenderContext &ctx_r)
{
    ZoneScoped;

    ctx.proj = perspective(ctx.fov, 1.f, ctx.near, ctx.far);
    ctx.proj_inv = inverse(ctx.proj);

    const array<mat4, 6> views{
        lookAt(position, position + vec3{1.f, 0.f, 0.f}, vec3{0.f, -1.f, 0.f}),
        lookAt(position, position + vec3{-1.f, 0.f, 0.f}, vec3{0.f, -1.f, 0.f}),
        lookAt(position, position + vec3{0.f, 1.f, 0.f}, vec3{0.f, 0.f, 1.f}),
        lookAt(position, position + vec3{0.f, -1.f, 0.f}, vec3{0.f, 0.f, -1.f}),
        lookAt(position, position + vec3{0.f, 0.f, 1.f}, vec3{0.f, -1.f, 0.f}),
        lookAt(position, position + vec3{0.f, 0.f, -1.f}, vec3{0.f, -1.f, 0.f}),
    };

    for (int face_idx = 0; face_idx < 6; face_idx++)
    {
        glNamedFramebufferTextureLayer(ctx.hdr_frame_buf, GL_COLOR_ATTACHMENT0,
                                       ctx.hdr_tex, 0, face_idx);
        ctx.view = views[face_idx];
        ctx.view_inv = glm::inverse(ctx.view);
        ctx.view_proj = ctx.proj * ctx.view;

        shadow.render(ctx, ctx_r);
        geometry.render({
            .size = ctx.size,
            .framebuf = ctx.g_buf.framebuffer,
            .view = ctx.view,
            .view_proj = ctx.view_proj,
            .view_proj_prev = ctx.view_proj_prev,
            .entity_vao = ctx_r.entity_vao,
            .sphere_mesh = ctx_r.mesh_instances[ctx_r.sphere_mesh_idx],
            .entities = ctx_r.queue,
            .meshes = ctx_r.mesh_instances,
            .lights = ctx_r.lights,
        });

        lighting.render(ctx, ctx_r);
        forward.render(ctx, ctx_r);
    }
}

void ProbeViewport::bake(RenderContext &ctx_r, const BakingJob &job,
                         int bounce_idx, const ProbeGrid &grid,
                         ProbeBuffer &buf)
{
    ZoneScoped;

    {
        TracyGpuZone("Probe rendering");

        if (bounce_idx == 0)
            ctx_r.probes.emplace_back(position);

        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                        GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glUseProgram(project.get_id());
        project.set("u_weight_sum", grid.weight_sum);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, grid.coef_buf);
        glBindImageTexture(0, ctx.hdr_tex, 0, true, 0, GL_READ_ONLY,
                           GL_RGBA16F);

        glDispatchCompute(grid.group_count.x, grid.group_count.y,
                          grid.group_count.z);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    {

        TracyGpuZone("Probe reduction");

        glUseProgram(reduce.get_id());
        reduce.set("u_idx", job.coords);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, grid.coef_buf);
        for (int j = 0; j < ProbeBuffer::count; j++)
            glBindImageTexture(j, buf.back()[j], 0, true, 0, GL_WRITE_ONLY,
                               GL_RGBA16F);

        glDispatchCompute(1, 1, 1);
    }
}