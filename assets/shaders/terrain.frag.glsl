#version 330

in vec2 tex_pos;
in vec2 grid_pos;
in vec3 world_pos;

layout(location=0) out vec4 out_col;

uniform sampler2D tex;
uniform bool show_grid;

// Fog of war: red channel 0 unexplored, 0.5 explored, 1 visible, one texel
// per tile with u along se and v along ne.
uniform sampler2D fog;
uniform bool fog_enabled;
uniform float fog_map_size;

void main()
{
    vec4 tex_val = texture(tex, tex_pos);
	if (show_grid) {
		vec2 distance_to_line = abs(fract(grid_pos - 0.5) - 0.5) / fwidth(grid_pos);
		float line = 1.0 - min(min(distance_to_line.x, distance_to_line.y), 1.0);
		tex_val.rgb = mix(tex_val.rgb, vec3(0.08, 0.12, 0.06), line * 0.55);
	}
	if (fog_enabled) {
		// World space is (se, up, -ne).
		vec2 fog_uv = vec2(world_pos.x, -world_pos.z) / fog_map_size;
		float seen = texture(fog, fog_uv).r;
		// Never-seen ground is black, explored ground dimmed to 45%;
		// piecewise linear so filtered tile edges blend smoothly.
		tex_val.rgb *= seen < 0.5 ? seen * 0.9 : 0.45 + (seen - 0.5) * 1.1;
	}
	out_col = tex_val;
}
