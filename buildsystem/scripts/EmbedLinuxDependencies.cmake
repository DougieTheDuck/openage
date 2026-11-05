# Copyright 2026 the openage authors. See copying.md for legal info.

# Runs at install/CPack time for LINUX_BUNDLE builds: copies the Python
# interpreter, Qt plugins/QML modules and every non-system shared library into
# the install tree so the result runs on other distros via run.sh.

if(NOT forward_linux_variables)
	message(FATAL_ERROR "CMake configuration variables not available. Please include(ForwardLinuxVariables.cmake)")
endif()

execute_process(
	COMMAND "${python}" "${buildsystem_dir}/scripts/bundle_linux.py"
		--prefix "${CMAKE_INSTALL_PREFIX}"
		--libdir "${install_libdir}"
		--asset-dir "${asset_dir}"
		--qt-plugins "${qt_plugins_dir}"
		--qt-qml "${qt_qml_dir}"
		--qmlimportscanner "${qmlimportscanner}"
	RESULT_VARIABLE bundle_result
)
if(NOT bundle_result EQUAL 0)
	message(FATAL_ERROR "Linux bundling failed with exit code ${bundle_result}.")
endif()
