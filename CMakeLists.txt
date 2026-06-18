cmake_minimum_required(VERSION 3.16)
project(particle_flow LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(APP_SOURCES
    src/app.c
    src/input.c
    src/main.c
    src/particle.c
    src/render.c
    src/gui.cpp
)

set(IMGUI_SOURCES
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/imgui_impl_sdl2.cpp
    third_party/imgui/imgui_impl_opengl3.cpp
)

add_executable(particle_flow ${APP_SOURCES} ${IMGUI_SOURCES})

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(particle_flow PRIVATE /O2 /fp:fast /GL)
        target_link_options(particle_flow PRIVATE /LTCG)
    else()
        target_compile_options(particle_flow PRIVATE
            -O3 -march=native -ffast-math -funroll-loops)
    endif()
endif()

find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

target_include_directories(particle_flow PRIVATE
    src
    third_party
    third_party/imgui
    ${SDL2_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
)

if(TARGET SDL2::SDL2)
    target_link_libraries(particle_flow PRIVATE SDL2::SDL2)
else()
    target_link_libraries(particle_flow PRIVATE ${SDL2_LIBRARIES})
endif()

if(TARGET GLEW::GLEW)
    target_link_libraries(particle_flow PRIVATE GLEW::GLEW)
else()
    target_link_libraries(particle_flow PRIVATE ${GLEW_LIBRARIES})
endif()

if(TARGET OpenGL::GL)
    target_link_libraries(particle_flow PRIVATE OpenGL::GL)
else()
    target_link_libraries(particle_flow PRIVATE ${OPENGL_LIBRARIES})
endif()

if(NOT WIN32)
    target_link_libraries(particle_flow PRIVATE m)
endif()

if(WIN32)
    set_target_properties(particle_flow PROPERTIES WIN32_EXECUTABLE TRUE)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(particle_flow PRIVATE SDL2::SDL2main)
    endif()
endif()
