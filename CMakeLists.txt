cmake_minimum_required(VERSION 3.20)
project(FnFNitroInFH2 VERSION 1.0.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the platform submodule (gives fh2mod_api and fh2mod_platform)
add_subdirectory(platform)

add_library(FnFNitro SHARED
    src/nitro_mod.cpp
    src/version.rc
)

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
