# Copyright 2026-2026 the openage authors. See copying.md for legal info.

install(DIRECTORY "graphics" "data" "qml"
	DESTINATION "${ASSET_DIR}/fvd"
)

install(FILES "modpack.toml"
	DESTINATION "${ASSET_DIR}/fvd"
)
