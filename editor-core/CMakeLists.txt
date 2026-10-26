cmake_minimum_required(VERSION 3.22)
project(a3d_editor_core LANGUAGES CXX)

add_library(a3d_editor_core STATIC
    src/editor_core.cpp
    src/editable_mesh.cpp
)
target_include_directories(a3d_editor_core PUBLIC include)
target_compile_features(a3d_editor_core PUBLIC cxx_std_20)
target_compile_options(a3d_editor_core PRIVATE -Wall -Wextra -Werror)

if(NOT ANDROID)
    enable_testing()
    add_executable(a3d_editor_core_test tests/editor_core_test.cpp)
    target_link_libraries(a3d_editor_core_test PRIVATE a3d_editor_core)
    add_test(NAME editor_core COMMAND a3d_editor_core_test)

    add_executable(a3d_editable_mesh_test tests/editable_mesh_test.cpp)
    target_link_libraries(a3d_editable_mesh_test PRIVATE a3d_editor_core)
    add_test(NAME editable_mesh COMMAND a3d_editable_mesh_test)
endif()
