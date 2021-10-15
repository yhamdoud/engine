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

LightingPass::LightingPass(LightingConfig params) : params(params)

{
    lighting_shader = *Shader::from_paths(
        ShaderPaths{
            .vert = shaders_path / "lighting.vs",
            .frag = shaders_path / "lighting.fs",
        },
        {
            .frag =
                fmt::format("#define CASCADE_COUNT {}", params.cascade_count),
        });

    lighting_shader.set("u_g_depth", 0);
    lighting_shader.set("u_g_normal_metallic", 1);
    lighting_shader.set("u_g_base_color_roughness", 2);
    lighting_shader.set("u_shadow_map", 3);
    lighting_shader.set("u_ao", 4);
    lighting_shader.set("u_sh_0", 5);
    lighting_shader.set("u_sh_1", 6);
    lighting_shader.set("u_sh_2", 7);
    lighting_shader.set("u_sh_3", 8);
    lighting_shader.set("u_sh_4", 9);
    lighting_shader.set("u_sh_5", 10);
    lighting_shader.set("u_sh_6", 11);

    parse_parameters();
}

void LightingPass::initialize(ViewportContext &ctx) {}

void LightingPass::parse_parameters()
{
    lighting_shader.set("u_use_direct", params.direct_light);
    lighting_shader.set("u_leak_offset", params.leak_offset);
    lighting_shader.set("u_use_base_color", params.use_base_color);
    lighting_shader.set("u_color_cascades", params.color_shadow_cascades);
    lighting_shader.set("u_filter", params.filter_shadows);
}

void LightingPass::render(ViewportContext &ctx_v, RenderContext &ctx_r)
{
    ZoneScoped;

    glViewport(0, 0, ctx_v.size.x, ctx_v.size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx_v.hdr_frame_buf);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(lighting_shader.get_id());
    lighting_shader.set("u_proj", ctx_v.proj);
    lighting_shader.set("u_proj_inv", ctx_v.proj_inv);
    lighting_shader.set("u_view_inv", inverse(ctx_v.view));

    lighting_shader.set("u_ssao", ssao);
    lighting_shader.set("u_ssr", ssr);

    glBindTextureUnit(0, ctx_v.g_buf.depth);
    glBindTextureUnit(1, ctx_v.g_buf.normal_metallic);
    glBindTextureUnit(2, ctx_v.g_buf.base_color_roughness);
    glBindTextureUnit(3, ctx_v.shadow_map);

    glBindTextureUnit(12, ctx_v.hdr_prev_tex);
    glBindTextureUnit(13, ctx_v.reflections_tex);
    glBindTextureUnit(14, ctx_v.g_buf.velocity);

    if (ctx_v.ao_tex != invalid_texture_id)
    {
        glBindTextureUnit(4, ctx_v.ao_tex);
    }
    else
    {
        lighting_shader.set("u_ssao", false);
    }

    if (ctx_r.sh_texs.size() != 0 && params.indirect_light)
    {
        lighting_shader.set("u_inv_grid_transform", ctx_r.inv_grid_transform);
        lighting_shader.set("u_grid_dims", ctx_r.grid_dims);

        glBindTextureUnit(5, ctx_r.sh_texs[0]);
        glBindTextureUnit(6, ctx_r.sh_texs[1]);
        glBindTextureUnit(7, ctx_r.sh_texs[2]);
        glBindTextureUnit(8, ctx_r.sh_texs[3]);
        glBindTextureUnit(9, ctx_r.sh_texs[4]);
        glBindTextureUnit(10, ctx_r.sh_texs[5]);
        glBindTextureUnit(11, ctx_r.sh_texs[6]);

        lighting_shader.set("u_use_irradiance", true);
    }
    else
    {
        lighting_shader.set("u_use_irradiance", false);
    }

    // TODO: Use a UBO or SSBO for this data.
    for (size_t i = 0; i < ctx_r.lights.size(); i++)
    {
        const auto &light = ctx_r.lights[i];
        vec4 pos = ctx_v.view * vec4(light.position, 1);
        lighting_shader.set(fmt::format("u_lights[{}].position", i),
                            vec3(pos) / pos.w);
        lighting_shader.set(fmt::format("u_lights[{}].color", i), light.color);
    }

    lighting_shader.set("u_light_transforms[0]", span(ctx_v.light_transforms));
    if (params.cascade_count > 1)
        lighting_shader.set("u_cascade_distances[0]",
                            span(ctx_v.cascade_distances));

    lighting_shader.set("u_directional_light.direction",
                        vec3{ctx_v.view * vec4{ctx_r.sun.direction, 0.f}});
    lighting_shader.set("u_directional_light.intensity",
                        ctx_r.sun.intensity * ctx_r.sun.color);

    glDrawArrays(GL_TRIANGLES, 0, 3);
}