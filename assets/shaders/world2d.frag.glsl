#version 330

in vec2 vert_uv;

layout(location=0) out vec4 col;

uniform sampler2D tex;
uniform bool cbits = false;

// position (top left corner) and size: (x, y, width, height)
uniform vec4 tile_params;

vec2 uv = vec2(
	vert_uv.x * tile_params.z + tile_params.x,
	vert_uv.y * tile_params.w + tile_params.y
);

void main() {
	vec4 tex_val = texture(tex, uv);
	int alpha = int(round(tex_val.a * 255));
	// Texture export noise must not write depth through transparent sprite padding.
	// Keep this cutoff synchronized with ALPHA_MASK_CUTOFF in texture_data.h.
	if (alpha <= 20) {
		discard;
	}
	// Translucent shadows and effects belong behind opaque sprite pixels.
	// Moving their depth to the far plane keeps them visible on empty ground
	// while allowing every ordinary sprite fragment to render in front.
	gl_FragDepth = alpha < 128 ? 1.0 : gl_FragCoord.z;
	if (!cbits) {
		col = tex_val;
		return;
	}
	switch (alpha) {
		case 254:
			col = vec4(1.0f, 0.0f, 0.0f, 1.0f);
			break;
		case 252:
			col = vec4(0.0f, 1.0f, 0.0f, 1.0f);
			break;
		case 250:
			col = vec4(0.0f, 0.0f, 1.0f, 1.0f);
			break;
		default:
			col = tex_val;
			break;
	}
}
