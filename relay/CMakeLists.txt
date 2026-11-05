# Copyright 2026 the openage authors. See copying.md for legal info.

# Standalone FVD room relay for headless Linux servers.
#
# A separate, small CMake project: it compiles only the relay and the few
# libopenage files it uses, so it needs a C++20 compiler and nothing else
# (no Qt, nyan, OpenGL or audio; Python 3 + mako only to generate the
# coord headers at build time).
# See doc/relay_server_linux.md.
#
#   cmake -S relay -B build-relay -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-relay
#   cd build-relay && cpack            # -> openage-relay-<version>-Linux-x86_64.tar.gz

cmake_minimum_required(VERSION 3.16)

set(OPENAGE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
file(STRINGS "${OPENAGE_ROOT}/openage_version" OPENAGE_RELAY_VERSION LIMIT_COUNT 1)

project(openage-relay VERSION ${OPENAGE_RELAY_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

# libopenage's config.h, with every optional engine feature switched off.
set(OPENAGE_SRC "${OPENAGE_ROOT}/libopenage")
set(AUTOGEN_WARNING "warning: auto-generated file by cmake. look at the template file instead.")
set(HAVE_THREAD_LOCAL_STORAGE true)
foreach(feature BACKTRACE INOTIFY OPENGL VULKAN GPERFTOOLS_PROFILER GPERFTOOLS_TCMALLOC NCURSES)
	set(WITH_${feature} false)
endforeach()
configure_file("${OPENAGE_SRC}/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/generated/config.h")

# network/messages.h includes the generated coordinate types. Generating them
# needs Python 3 with mako at build time only; the relay binary doesn't use Python.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
	COMMAND "${Python3_EXECUTABLE}" -c "import mako"
	RESULT_VARIABLE mako_missing
	OUTPUT_QUIET ERROR_QUIET
)
if(mako_missing)
	message(FATAL_ERROR
		"The relay build needs the Python 'mako' module to generate coord headers "
		"(Debian/Ubuntu: python3-mako, Fedora: python3-mako, Arch: python-mako, or pip install mako).")
endif()
set(coord_templates
	"${OPENAGE_SRC}/coord/coord.h.template"
	"${OPENAGE_SRC}/coord/coord.cpp.template"
	"${OPENAGE_ROOT}/openage/codegen/coord.py"
	"${CMAKE_CURRENT_SOURCE_DIR}/gen_coord_headers.py"
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${coord_templates})
execute_process(
	COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/gen_coord_headers.py"
	        "${OPENAGE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/generated"
	RESULT_VARIABLE codegen_failed
)
if(codegen_failed)
	message(FATAL_ERROR "Generating the coord headers failed.")
endif()

# The libopenage sources the relay links against. If a relay change pulls in
# another file, the link step reports the missing symbol: add its .cpp here.
set(RELAY_LIBOPENAGE_SOURCES
	error/backtrace.cpp
	error/error.cpp
	error/stackanalyzer.cpp
	log/level.cpp
	log/log.cpp
	log/logsink.cpp
	log/logsource.cpp
	log/message.cpp
	log/named_logsource.cpp
	log/stdout_logsink.cpp
	network/protocol.cpp
	network/relay.cpp
	network/relay_service.cpp
	network/serialization.cpp
	network/transport.cpp
	util/compiler.cpp
	util/stringformatter.cpp
	util/strings.cpp
	util/thread_id.cpp
	util/timing.cpp
)
list(TRANSFORM RELAY_LIBOPENAGE_SOURCES PREPEND "${OPENAGE_SRC}/")

add_executable(openage-relay main.cpp ${RELAY_LIBOPENAGE_SOURCES})
target_include_directories(openage-relay PRIVATE
	"${CMAKE_CURRENT_BINARY_DIR}/generated"
	"${CMAKE_CURRENT_BINARY_DIR}/generated/libopenage"
	"${CMAKE_CURRENT_BINARY_DIR}/generated/libopenage/coord"
	"${OPENAGE_SRC}"
)
target_link_libraries(openage-relay PRIVATE Threads::Threads)

# Bundle the C++ runtime so the binary only needs the system's glibc.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_link_options(openage-relay PRIVATE -static-libstdc++ -static-libgcc)
endif()

install(TARGETS openage-relay RUNTIME DESTINATION .)
install(FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/openage-relay.service"
	"${OPENAGE_ROOT}/doc/relay_server_linux.md"
	"${OPENAGE_ROOT}/copying.md"
	DESTINATION .
)

set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_NAME "openage-relay")
set(CPACK_PACKAGE_VENDOR "The openage authors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Headless FVD room relay server")
set(CPACK_PACKAGE_FILE_NAME "openage-relay-v${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
include(CPack)
