cmake_minimum_required(VERSION 3.22.1)
project(a3d_core LANGUAGES CXX)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../editor-core
    ${CMAKE_CURRENT_BINARY_DIR}/editor-core
)

add_library(a3d_core SHARED native_renderer.cpp)
target_compile_features(a3d_core PRIVATE cxx_std_20)
target_link_libraries(a3d_core PRIVATE a3d_editor_core GLESv3 log android)
