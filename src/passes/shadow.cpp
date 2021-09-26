#include "shadow.hpp"
#include "../render_context.hpp"
#include "../renderer.hpp"

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <numeric>

using namespace std;
using namespace glm;
using namespace engine;

static constexpr vec3 homogenize(const vec4 &v) { return vec3(v) / v.w; }

ShadowPass::ShadowPass(ShadowConfig cfg)
    : size(cfg.size), stabilize(cfg.stabilize)
{

    // Depth texture rendered from light perspective.
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &shadow_map);
    glCreateFramebuffers(1, &frame_buf);

    shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "shadow_map.vs",
        .geom = shaders_path / "shadow_map.geom",
    });
}

void ShadowPass::initialize(ViewportContext &ctx)
{
    // Split scheme. Source:
    // https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus
    float lambda = 0.6f;

    const auto &n = ctx.near;
    const auto &f = ctx.far;

    for (int i = 0; i < cascade_count; i++)
    {
        float s_i =
            static_cast<float>(i + 1) / static_cast<float>(cascade_count);
        float log = n * pow(f / n, s_i);
        float uni = n + (f - n) * s_i;
        cascade_distances[i] = lerp(uni, log, lambda);
    }

    glTextureParameteri(shadow_map, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadow_map, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadow_map, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(shadow_map, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(shadow_map, GL_TEXTURE_COMPARE_MODE,
                        GL_COMPARE_REF_TO_TEXTURE);

    // Areas beyond the coverage of the shadow map are in light.
    vec4 border_color{1.f};
    glTextureParameterfv(shadow_map, GL_TEXTURE_BORDER_COLOR,
                         value_ptr(border_color));

    glTextureStorage3D(shadow_map, 1, GL_DEPTH_COMPONENT32, size.x, size.y,
                       cascade_count);

    glGenTextures(cascade_count, debug_views.data());

    for (int i = 0; i < cascade_count; i++)
    {
        glTextureView(debug_views[i], GL_TEXTURE_2D, shadow_map,
                      GL_DEPTH_COMPONENT32, 0, 1, i, 1);
    }

    glNamedFramebufferTexture(frame_buf, GL_DEPTH_ATTACHMENT, shadow_map, 0);
    // We only care about the depth test.
    glNamedFramebufferDrawBuffer(frame_buf, GL_NONE);
    glNamedFramebufferReadBuffer(frame_buf, GL_NONE);

    ctx.cascade_distances = span(cascade_distances);
    ctx.light_transforms = span(light_transforms);
    ctx.shadow_map = shadow_map;
}

void ShadowPass::render(ViewportContext &ctx, RenderContext &ctx_r)
{
    glViewport(0, 0, size.x, size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, frame_buf);
    glClear(GL_DEPTH_BUFFER_BIT);

    float aspect_ratio =
        static_cast<float>(ctx.size.x) / static_cast<float>(ctx.size.y);

    for (int c_idx = 0; c_idx < cascade_count; c_idx++)
    {
        float near = (c_idx == 0) ? 0.1f : cascade_distances[c_idx - 1];
        float far = cascade_distances[c_idx];

        // Camera's projection matrix for the current cascade.
        const mat4 cascade_proj = perspective(ctx.fov, aspect_ratio, near, far);

        const mat4 inv = inverse(cascade_proj * ctx.view);

        // Camera's frustrum corners in world space.
        const array<vec3, 8> frustrum_corners{
            homogenize(inv * glm::vec4{-1.f, -1.f, -1.f, 1.f}),
            homogenize(inv * glm::vec4{-1.f, -1.f, 1.f, 1.f}),
            homogenize(inv * glm::vec4{-1.f, 1.f, -1.f, 1.f}),
            homogenize(inv * glm::vec4{-1.f, 1.f, 1.f, 1.f}),
            homogenize(inv * glm::vec4{1.f, -1.f, -1.f, 1.f}),
            homogenize(inv * glm::vec4{1.f, -1.f, 1.f, 1.f}),
            homogenize(inv * glm::vec4{1.f, 1.f, -1.f, 1.f}),
            homogenize(inv * glm::vec4{1.f, 1.f, 1.f, 1.f}),
        };

        const vec3 center = accumulate(frustrum_corners.begin(),
                                       frustrum_corners.end(), vec3(0.f)) /
                            8.f;

        vec3 min(numeric_limits<float>::max());
        vec3 max(numeric_limits<float>::min());

        float radius = 0;

        if (stabilize)
        {
            // Calculate the radius of the bounding sphere surrounding the
            // frustum.
            // TODO: We could get a tighter fit using something like:
            // https://lxjk.github.io/2017/04/15/Calculate-Minimal-Bounding-Sphere-of-Frustum.html
            for (const auto &corner : frustrum_corners)
                radius = glm::max(radius, length(corner - center));

            max = vec3(radius);
            min = -max;
        }
        else
        {
            // Construct temporary view matrix.
            mat4 view = lookAt(center - ctx_r.sun.direction, center,
                               vec3(0.f, 1.f, 0.f));

            // Transform the frustrum to light space and calculate a tight
            // fitting AABB.
            for (const auto &corner : frustrum_corners)
            {
                const vec3 corner_light_space = view * vec4(corner, 1.f);
                min = glm::min(min, corner_light_space);
                max = glm::max(max, corner_light_space);
            }

            // Include geometry that might be outside the frustrum but
            // contributes to lighting.
            const float z_mult = 15.f;
            const float z_mult_inv = 1 / z_mult;

            min.z *= (min.z < 0) ? z_mult : z_mult_inv;
            max.z *= (max.z < 0) ? z_mult_inv : z_mult;
        }

        mat4 light_view = lookAt(center - ctx_r.sun.direction * -min.z, center,
                                 vec3(0.f, 1.f, 0.f));

        // Snap to shadow map texels.
        if (stabilize)
        {
            light_view[3].x -= fmodf(light_view[3].x, (radius / size.x) * 2.0f);
            light_view[3].y -= fmodf(light_view[3].y, (radius / size.x) * 2.0f);
            // TODO:
            // light_view[3].z -= ...
        }

        mat4 light_proj =
            glm::ortho(min.x, max.y, min.y, max.y, 0.f, max.z - min.z);

        light_transforms[c_idx] = light_proj * light_view;
    }

    glUseProgram(shader.get_id());

    shader.set("u_light_transforms[0]", span(light_transforms));

    glBindVertexArray(ctx_r.entity_vao);

    glCullFace(GL_FRONT); // Peter panning.

    for (auto &r : ctx_r.queue)
    {
        if (r.flags & Entity::casts_shadow)
        {
            shader.set("u_model", r.model);
            Renderer::render_mesh_instance(ctx_r.entity_vao,
                                           ctx_r.mesh_instances[r.mesh_index]);
        }
    }

    glCullFace(GL_BACK);
}