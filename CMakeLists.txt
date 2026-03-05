cmake_minimum_required(VERSION 3.16)
project(particle_sim C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- ソース一覧 ----
file(GLOB SRCS "src/*.c")

add_executable(particle_sim ${SRCS})

# ---- Release 最適化 (cmake -DCMAKE_BUILD_TYPE=Release ..) ----
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(particle_sim PRIVATE /O2 /fp:fast /GL)
        target_link_options(particle_sim PRIVATE /LTCG)
    else()
        target_compile_options(particle_sim PRIVATE
            -O3 -march=native -ffast-math -funroll-loops)
    endif()
endif()

# ---- 依存ライブラリ検索 ----
find_package(SDL2  REQUIRED)
find_package(GLEW  REQUIRED)
find_package(OpenGL REQUIRED)

target_include_directories(particle_sim PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    src)

target_link_libraries(particle_sim PRIVATE
    ${SDL2_LIBRARIES}
    GLEW::GLEW
    ${OPENGL_LIBRARIES}
    m)

# Windows: コンソールを非表示にして SDL のウィンドウを前面に
if(WIN32)
    set_target_properties(particle_sim PROPERTIES
        WIN32_EXECUTABLE TRUE)
    target_link_libraries(particle_sim PRIVATE SDL2::SDL2main)
endif()
