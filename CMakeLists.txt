cmake_minimum_required(VERSION 3.10)
project(OpenGLProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include glad.c so it actually gets compiled
add_executable(main
    src/main.cpp
    src/glad.c
    src/Sphere.cpp
)

# Make sure we tell CMake about BOTH include/glad and include/GLFW
target_include_directories(main PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Link libraries: 
#  1) The static GLFW library in /lib 
target_link_libraries(main PRIVATE
"${CMAKE_CURRENT_SOURCE_DIR}/lib/libglfw3.a"
)