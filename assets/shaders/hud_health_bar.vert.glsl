#version 330

layout(location=0) in vec2 position;

layout (std140) uniform camera {
    mat4 view;
    mat4 proj;
    float inv_zoom;
    vec2 inv_viewport_size;
};

uniform vec3 obj_world_position;
uniform vec2 bar_size;
uniform vec2 bar_offset;

void main() {
    vec4 anchor = proj * view * vec4(obj_world_position, 1.0);
    vec2 offset = vec2(bar_offset.x, bar_offset.y * inv_zoom);
    vec2 pixel_position = 2.0 * offset + position * bar_size;
    anchor.xy += pixel_position * inv_viewport_size * anchor.w;
    gl_Position = anchor;
}
