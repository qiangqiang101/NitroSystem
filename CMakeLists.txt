cmake_minimum_required(VERSION 3.20)
project(FnFNitroInFH2 VERSION 1.3.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the platform submodule for its header (fh2mod_api). The platform DLL itself is not built here: it needs llvm-mingw,
# so dist/d3dcompiler_47.dll is the official release build (see README)
add_subdirectory(platform EXCLUDE_FROM_ALL)

add_library(FnFNitro SHARED
    src/nitro_mod.cpp
    src/version.rc
    src/hud.rc
)

# rc files don't track what they embed, so changing a HUD file would not rebuild the DLL
file(GLOB_RECURSE HUD_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/hud/* ${CMAKE_SOURCE_DIR}/particles/* ${CMAKE_SOURCE_DIR}/sounds/*)
set_source_files_properties(src/hud.rc PROPERTIES OBJECT_DEPENDS "${HUD_FILES}")

target_link_libraries(FnFNitro PRIVATE fh2mod_api user32 xinput winmm)
set_target_properties(FnFNitro PROPERTIES OUTPUT_NAME FnFNitro PREFIX "")
target_compile_definitions(FnFNitro PRIVATE
    FNFNITRO_VERSION="${PROJECT_VERSION}"
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(FnFNitro PRIVATE /W4 /permissive- /utf-8 /EHsc)
else()
    target_compile_options(FnFNitro PRIVATE -Wall -Wextra)
    target_link_options(FnFNitro PRIVATE -static)
endif()
