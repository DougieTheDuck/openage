#version 330

in vec2 offset_px;
in vec2 ellipse_radius_px;

layout(location = 0) out vec4 col;

// The signed distance of a point from an ellipse, locally converted to
// screen pixels. A screen-space stroke stays ~2px wide at any zoom.
float distance_to_ellipse_px(vec2 position_px) {
    vec2 scaled = position_px / ellipse_radius_px;
    float radial = length(scaled);
    vec2 normal = scaled / max(radial, 0.00001);
    float gradient = length(normal / ellipse_radius_px);
    return (radial - 1.0) / max(gradient, 0.00001);
}

float stroke_coverage(float distance_px) {
    return 1.0 - smoothstep(0.75, 1.75, abs(distance_px));
}

void main() {
    // The shadow is the same ring shifted down one physical screen pixel.
    // Draw it first, then the white stroke covers the overlap.
    float shadow = stroke_coverage(
        distance_to_ellipse_px(offset_px + vec2(0.0, 1.0)));
    float white = stroke_coverage(distance_to_ellipse_px(offset_px));
    float alpha = white + shadow * (1.0 - white);
    if (alpha <= 0.001) discard;

    vec3 premultiplied = vec3(0.025) * shadow * (1.0 - white)
                       + vec3(1.0) * white;
    col = vec4(premultiplied / alpha, alpha);
}
