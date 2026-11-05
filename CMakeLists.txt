cmake_minimum_required(VERSION 3.15)
project(cg CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(cg main.cpp external/glad/src/gl.c)

target_include_directories(cg PRIVATE external/glad/include external/stb)

target_link_libraries(cg PRIVATE glfw OpenGL::GL ${CMAKE_DL_LIBS})

set_target_properties(cg PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
