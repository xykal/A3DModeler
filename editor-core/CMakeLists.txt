cmake_minimum_required(VERSION 3.22)
project(a3d_editor_core LANGUAGES CXX)

add_library(a3d_editor_core STATIC
    src/editor_core.cpp
    src/editable_mesh.cpp
    src/modeling_scene.cpp
    src/project_io.cpp
    src/obj_io.cpp
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

    add_executable(a3d_modeling_scene_test tests/modeling_scene_test.cpp)
    target_link_libraries(a3d_modeling_scene_test PRIVATE a3d_editor_core)
    add_test(NAME modeling_scene COMMAND a3d_modeling_scene_test)

    add_executable(a3d_project_io_test tests/project_io_test.cpp)
    target_link_libraries(a3d_project_io_test PRIVATE a3d_editor_core)
    add_test(NAME project_io COMMAND a3d_project_io_test)

    add_executable(a3d_obj_io_test tests/obj_io_test.cpp)
    target_link_libraries(a3d_obj_io_test PRIVATE a3d_editor_core)
    add_test(NAME obj_io COMMAND a3d_obj_io_test)
endif()
