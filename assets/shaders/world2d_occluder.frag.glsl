#version 330

in vec2 vert_uv;

layout(location=0) out vec4 col;

uniform sampler2D tex;

// position (top left corner) and size: (x, y, width, height)
uniform vec4 tile_params;

void main() {
	vec2 uv = vec2(
		vert_uv.x * tile_params.z + tile_params.x,
		vert_uv.y * tile_params.w + tile_params.y
	);
	int alpha = int(round(texture(tex, uv).a * 255));
	// Shadows and transparent padding must not hide units behind a structure.
	if (alpha < 128) {
		discard;
	}
	col = vec4(0.0);
}
