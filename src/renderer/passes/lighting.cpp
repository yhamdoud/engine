#include <cmath>

#include <Tracy.hpp>
#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include "constants.hpp"
#include "lighting.hpp"
#include "logger.hpp"
#include "model.hpp"
#include "renderer/renderer.hpp"

using namespace std;
using namespace glm;
using namespace engine;

LightingPass::LightingPass(Params params) : params(params)

{
    glCreateBuffers(1, &uniform_buf);
    glNamedBufferData(uniform_buf, sizeof(Uniforms), nullptr, GL_DYNAMIC_DRAW);

    lighting_shader = *Shader::from_paths(
        ShaderPaths{
            .vert = shaders_path / "lighting.vs",
            .frag = shaders_path / "lighting.fs",
        },
        {
            .frag =
                fmt::format("#define CASCADE_COUNT {}", params.cascade_count),
        });

    parse_parameters();
}

void LightingPass::initialize(ViewportContext &ctx) {}

void LightingPass::parse_parameters()
{
    uniforms = {
        .leak_offset = params.leak_offset,
        .direct_lighting = params.direct_lighting,
        .base_color = params.base_color,
        .color_cascades = params.color_cascades,
        .filter_shadows = params.filter_shadows,
    };
}

void LightingPass::render(ViewportContext &ctx_v, RenderContext &ctx_r)
{
    ZoneScoped;

    uniforms.proj = ctx_v.proj;
    uniforms.view_inv = inverse(ctx_v.view);
    uniforms.reflections = params.ssr;
    uniforms.ambient_occlusion = params.ssao;
    uniforms.inv_grid_transform = ctx_r.inv_grid_transform;
    uniforms.grid_dims = ctx_r.grid_dims;
    uniforms.indirect_lighting =
        ctx_r.sh_texs.size() != 0 && params.indirect_light;
    uniforms.light_intensity = ctx_r.sun.intensity * ctx_r.sun.color;
    uniforms.light_direction =
        vec3{ctx_v.view * vec4{ctx_r.sun.direction, 0.f}};
    uniforms.proj_inv = ctx_v.proj_inv;

    glNamedBufferSubData(uniform_buf, 0, sizeof(Uniforms), &uniforms);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniform_buf);

    lighting_shader.set("u_proj_inv", ctx_v.proj_inv);

    glBindTextureUnit(0, ctx_v.g_buf.depth);
    glBindTextureUnit(1, ctx_v.g_buf.normal_metallic);
    glBindTextureUnit(2, ctx_v.g_buf.base_color_roughness);
    glBindTextureUnit(3, ctx_v.shadow_map);
    glBindTextureUnit(4, ctx_v.ao_tex);
    glBindTextureUnit(5, ctx_r.sh_texs[0]);
    glBindTextureUnit(6, ctx_r.sh_texs[1]);
    glBindTextureUnit(7, ctx_r.sh_texs[2]);
    glBindTextureUnit(8, ctx_r.sh_texs[3]);
    glBindTextureUnit(9, ctx_r.sh_texs[4]);
    glBindTextureUnit(10, ctx_r.sh_texs[5]);
    glBindTextureUnit(11, ctx_r.sh_texs[6]);
    glBindTextureUnit(12, ctx_v.history_tex);
    glBindTextureUnit(13, ctx_v.reflections_tex);
    glBindTextureUnit(14, ctx_v.g_buf.velocity);
    glBindTextureUnit(15, ctx_r.light_shadows_array);

    // TODO: UBO
    lighting_shader.set("u_light_transforms[0]", span(ctx_v.light_transforms));
    if (params.cascade_count > 1)
        lighting_shader.set("u_cascade_distances[0]",
                            span(ctx_v.cascade_distances));

    glDisable(GL_DEPTH_TEST);

    glViewport(0, 0, ctx_v.size.x, ctx_v.size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx_v.hdr_frame_buf);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(lighting_shader.get_id());
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glUseProgram(point_light_shader.get_id());

    point_light_shader.set("u_view", ctx_v.view);
    point_light_shader.set("u_view_proj", ctx_v.view_proj);

    glCullFace(GL_FRONT);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);

    // TODO: instancing
    if (params.direct_lighting)
    {
        int i = 0;
        for (const auto &light : ctx_r.lights)
        {
            const float radius_squared = light.radius_squared(0.01f);

            point_light_shader.set("u_light_idx", i);
            point_light_shader.set("u_light.position", light.position);
            point_light_shader.set("u_light.color",
                                   light.intensity * light.color);
            point_light_shader.set("u_light.radius", sqrt(radius_squared));
            point_light_shader.set("u_light.radius_squared", radius_squared);

            Renderer::render_mesh_instance(
                ctx_r.mesh_instances[ctx_r.sphere_mesh_idx]);
            i++;
        }
    }

    glDisable(GL_BLEND);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
}