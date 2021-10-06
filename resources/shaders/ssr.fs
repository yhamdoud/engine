#version 460 core

in vec2 tex_coords;
in vec3 view_ray;

layout (location = 0) out vec4 frag_color;

layout (binding = 0) uniform sampler2D u_g_depth;
layout (binding = 1) uniform sampler2D u_g_normal_metallic;
layout (binding = 2) uniform sampler2D u_g_base_color_roughness;
layout (binding = 3) uniform sampler2D u_hdr_target;

uniform mat4 u_proj;
uniform float u_near;

// Camera space thickness to ascribe to each pixel in the depth buffer.
uniform float u_thickness;
// Step in horizontal or vertical pixels between samples.
uniform float u_stride;
uniform bool u_do_jitter;
// Maximum number of iterations. Higher gives better images but may be slow.
uniform int u_max_steps;
// Maximum camera-space distance to trace before returning a miss.
uniform float u_max_dist;

float linearize_depth(float depth, mat4 proj)
{
    return -proj[3][2] / (2. * depth - 1. + proj[2][2]);
}

void swap(inout float a, inout float b)
{
    float t = a;
    a = b;
    b = t;
}

bool intersects_depth(float z, float z_min, float z_max)
{
    return (z_max >= z - u_thickness) && (z_min < z);
}

float distance_squared(vec2 a, vec2 b)
{
    a -= b;
    return dot(a, a);
}

// Returns true if the ray hit something.
// 
// Modified version of the following work:
// https://casual-effects.blogspot.com/2014/08/screen-space-ray-tracing.html
//
// By Morgan McGuire and Michael Mara at Williams College 2014
// Released as open source under the BSD 2-Clause License
// http://opensource.org/licenses/BSD-2-Clause
bool ray_trace(vec3 origin, vec3 dir, float jitter, vec2 z_buf_size,
               float z_near, out vec2 hit_screen, out vec3 hit_view)
{
    // Clip to the near plane.
    float ray_length = (origin.z + dir.z * u_max_dist) > z_near
        ? (z_near - origin.z) / dir.z
        : u_max_dist;
    vec3 end = origin + dir * ray_length;

    // Project into homogeneous clip space.
    vec4 origin_clip = u_proj * vec4(origin, 1.0);
    vec4 end_clip = u_proj * vec4(end, 1.0);

    // Because the caller was required to clip to the near plane,
    // this homogeneous division (projecting from 4D to 2D) is guaranteed 
    // to succeed. 
    float k0 = 1.0 / origin_clip.w;
    float k1 = 1.0 / end_clip.w;

    // The interpolated homogeneous version of the camera-space points  
    vec3 q0 = origin * k0;
    vec3 q1 = end * k1;

    // Screen-space endpoints
    vec2 p0 = origin_clip.xy * k0;
    vec2 p1 = end_clip.xy * k1;
    p0 = (p0 * 0.5 + 0.5) * z_buf_size;
    p1 = (p1 * 0.5 + 0.5) * z_buf_size;

    // If the line is degenerate, make it cover at least one pixel
    // to avoid handling zero-pixel extent as a special case later
    p1 += vec2((distance_squared(p0, p1) < 0.0001) ? 0.01 : 0.0);
    vec2 delta = p1 - p0;

    // Permute so that the primary iteration is in x to collapse all
    // quadrant-specific DDA cases later.
    bool permute = false;
    if (abs(delta.x) < abs(delta.y))
    { 
        // This is a more-vertical line.
        permute = true;
        delta = delta.yx;
        p0 = p0.yx;
        p1 = p1.yx; 
    }

    // From now on, x is the primary iteration direction and y is the secondary one.

    float step_dir = sign(delta.x);
    float invdx = step_dir / delta.x;

    // Track the derivatives of q and k
    vec3  dq = (q1 - q0) * invdx;
    float dk = (k1 - k0) * invdx;
    vec2  dp = vec2(step_dir, delta.y * invdx);

    // Scale derivatives by the desired pixel stride.
    dp *= u_stride;
    dq *= u_stride;
    dk *= u_stride;

    // Offset the starting values by the jitter fraction.
    p0 += dp * jitter;
    q0 += dq * jitter;
    k0 += dk * jitter;

    // Slide p from p0 to p1, (now-homogeneous) q from q0 to q1, k from k0 to k1
    vec3 q = q0; 

    // Adjust end condition for iteration direction
    float end_dir = p1.x * step_dir;

    float k = k0;
    float step_count = 0.0;
    float z_max_prev = origin.z;

    // Interval of the ray.
    float z_min = z_max_prev;
    float z_max = z_max_prev;

    float z_max_scene = z_max + 100;

    for (vec2 p = p0; 
         ((p.x * step_dir) <= end_dir)
         && (step_count < u_max_steps)
         && !intersects_depth(z_max_scene, z_min, z_max)
         && (z_max_scene != 0); 
         p += dp, q.z += dq.z, k += dk, step_count++)
    {
        z_min = z_max_prev;

        // Compute the value at 1/2 pixel into the future.
        z_max = (dq.z * 0.5 + q.z) / (dk * 0.5 + k);
        z_max_prev = z_max;
        if (z_min > z_max) swap(z_min, z_max);

        hit_screen = permute ? p.yx : p;

        // View-space z of the background.
        // TODO: Might want to precompute a linear depth buffer for this.
        z_max_scene = linearize_depth(texelFetch(u_g_depth, ivec2(hit_screen), 0).r, u_proj);
    }
    
    // Advance q based on the number of steps.
    q.xy += dq.xy * step_count;
    hit_view = q * (1.0 / k);
    return intersects_depth(z_max_scene, z_min, z_max);
}

void main()
{
    float depth = texture(u_g_depth, tex_coords).r;
    if (depth == 1.0)
        discard;

    float roughness = texture(u_g_base_color_roughness, tex_coords).a;
    if (roughness > 0.9)
        discard;

    vec2 hit_screen = vec2(0., 0.);
    vec3 hit_view = vec3(0., 0., 0.);

    vec3 ray_origin = view_ray * linearize_depth(depth, u_proj);

    vec3 normal = texture(u_g_normal_metallic, tex_coords).xyz;

    float n_dot_v = max(dot(normal, normalize(-ray_origin)), 0.);

    vec3 ray_dir = normalize(reflect(normalize(ray_origin), normal));

    vec2 size = textureSize(u_g_depth, 0);

    ivec2 c = ivec2(gl_FragCoord.xy);
    // Number between 0 and 1 for how far to bump the ray in stride units
    // to conceal banding artifacts
    float jitter = u_stride > 1.f && u_do_jitter ? float((c.x + c.y) & 1) * 0.5 : 0.;

    bool hit = ray_trace(
        ray_origin + 0.1 * ray_dir,
        ray_dir,
        jitter,
        size,
        -u_near,
        hit_screen,
        hit_view
    );

    frag_color = hit
        ? vec4(texelFetch(u_hdr_target, ivec2(hit_screen), 0).rgb, n_dot_v)
        : vec4(0.f);
}