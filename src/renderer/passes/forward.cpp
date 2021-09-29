#include <Tracy.hpp>

#include "forward.hpp"
#include "renderer/renderer.hpp"

using namespace std;
using namespace glm;
using namespace engine;

ForwardPass::ForwardPass(ForwardConfig cfg) : draw_probes(cfg.draw_probes)
{
    probe_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "probe.vs",
        .frag = shaders_path / "probe.fs",
    });

    skybox_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "skybox.vs",
        .frag = shaders_path / "skybox.fs",
    });

    probe_shader.set("u_sh_0", 0);
    probe_shader.set("u_sh_1", 1);
    probe_shader.set("u_sh_2", 2);
    probe_shader.set("u_sh_3", 3);
    probe_shader.set("u_sh_4", 4);
    probe_shader.set("u_sh_5", 5);
    probe_shader.set("u_sh_6", 6);
}

void ForwardPass::parse_parameters() {}

void ForwardPass::initialize(ViewportContext &ctx) {}

void ForwardPass::render(ViewportContext &ctx_v, RenderContext &ctx_r)
{
    ZoneScoped;

    glViewport(0, 0, ctx_v.size.x, ctx_v.size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx_v.hdr_framebuf);
    glBlitNamedFramebuffer(ctx_v.g_buf.framebuffer, ctx_v.hdr_framebuf, 0, 0,
                           ctx_v.size.x, ctx_v.size.y, 0, 0, ctx_v.size.x,
                           ctx_v.size.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindVertexArray(ctx_r.skybox_vao);

    glUseProgram(skybox_shader.get_id());

    skybox_shader.set("u_projection", ctx_v.proj);
    skybox_shader.set("u_view", mat4(mat3(ctx_v.view)));

    glBindTextureUnit(0, ctx_r.skybox_tex);

    glDrawArrays(GL_TRIANGLES, 0, 36);

    if (draw_probes)
    {
        glBindVertexArray(ctx_r.entity_vao);

        glUseProgram(probe_shader.get_id());

        float far_clip_dist = ctx_v.proj[3][2] / (ctx_v.proj[2][2] + 1.f);
        probe_shader.set("u_far_clip_distance", far_clip_dist);

        probe_shader.set("u_inv_grid_transform", ctx_r.inv_grid_transform);
        probe_shader.set("u_grid_dims", ctx_r.grid_dims);

        // FIXME:
        if (ctx_r.sh_texs.size() == 0)
            return;

        glBindTextureUnit(0, ctx_r.sh_texs[0]);
        glBindTextureUnit(1, ctx_r.sh_texs[1]);
        glBindTextureUnit(2, ctx_r.sh_texs[2]);
        glBindTextureUnit(3, ctx_r.sh_texs[3]);
        glBindTextureUnit(4, ctx_r.sh_texs[4]);
        glBindTextureUnit(5, ctx_r.sh_texs[5]);
        glBindTextureUnit(6, ctx_r.sh_texs[6]);

        for (const auto &probe : ctx_r.probes)
        {
            const mat4 model =
                scale(translate(mat4(1.f), probe.position), vec3(0.2f));
            const mat4 model_view = ctx_v.view * model;

            probe_shader.set("u_model", model);
            probe_shader.set("u_model_view", model_view);
            probe_shader.set("u_mvp", ctx_v.proj * model_view);

            Renderer::render_mesh_instance(
                ctx_r.entity_vao, ctx_r.mesh_instances[ctx_r.probe_mesh_idx]);
        }
    }
}
