#version 330

in vec2 vert_uv;

layout(location=0) out vec4 col;

uniform sampler2D tex;

// position (top left corner) and size: (x, y, width, height)
uniform vec4 tile_params;

const float shadow_rgb_max = 8.0 / 255.0;

bool is_shadow(vec4 value, int alpha) {
	return alpha < 250
	    && max(max(value.r, value.g), value.b) <= shadow_rgb_max;
}

void main() {
	vec2 uv = vec2(
		vert_uv.x * tile_params.z + tile_params.x,
		vert_uv.y * tile_params.w + tile_params.y
	);
	vec4 tex_val = texture(tex, uv);
	int alpha = int(round(tex_val.a * 255));
	// Shadows and transparent padding must not hide units behind a structure.
	if (alpha < 128 || is_shadow(tex_val, alpha)) {
		discard;
	}
	col = vec4(0.0);
}
