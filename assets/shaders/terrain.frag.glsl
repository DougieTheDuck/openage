#version 330

in vec2 tex_pos;
in vec2 grid_pos;

layout(location=0) out vec4 out_col;

uniform sampler2D tex;
uniform bool show_grid;

void main()
{
    vec4 tex_val = texture(tex, tex_pos);
	if (show_grid) {
		vec2 distance_to_line = abs(fract(grid_pos - 0.5) - 0.5) / fwidth(grid_pos);
		float line = 1.0 - min(min(distance_to_line.x, distance_to_line.y), 1.0);
		tex_val.rgb = mix(tex_val.rgb, vec3(0.08, 0.12, 0.06), line * 0.55);
	}
	out_col = tex_val;
}
