#version 330

// A screen-aligned quad whose dimensions come from projecting a circular
// footprint on the world XZ ground plane through the actual game camera.
layout(location = 0) in vec2 v_position;
layout(location = 1) in vec2 uv;

layout(std140) uniform camera {
    mat4 view;
    mat4 proj;
    float inv_zoom;
    vec2 inv_viewport_size;
};

uniform vec3 unit_position;
uniform float footprint_radius;

out vec2 offset_px;
out vec2 ellipse_radius_px;

void main() {
    vec4 centre = proj * view * vec4(unit_position, 1.0);
    // Project two perpendicular unit-radius ground-plane axes. The row
    // lengths of this 2x2 projection give the exact ellipse radii, for
    // any orthographic isometric/dimetric camera rotation.
    vec2 se = (proj * view * vec4(footprint_radius, 0.0, 0.0, 0.0)).xy;
    vec2 ne = (proj * view * vec4(0.0, 0.0, footprint_radius, 0.0)).xy;
    vec2 radius_ndc = sqrt(se * se + ne * ne);
    ellipse_radius_px = max(radius_ndc / (2.0 * inv_viewport_size),
                            vec2(2.0));

    // Four spare pixels accommodate the anti-aliased white stroke and
    // the dark line one screen pixel below it.
    vec2 half_size_px = ellipse_radius_px + vec2(4.0);
    offset_px = v_position * half_size_px;
    gl_Position = centre + vec4(offset_px * (2.0 * inv_viewport_size),
                                 0.0, 0.0);
}
