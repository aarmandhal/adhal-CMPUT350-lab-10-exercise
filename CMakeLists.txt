cmake_minimum_required(VERSION 3.28)
project(shaders LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux"
    AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"
    AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "9.4.0")
    include(${CMAKE_SOURCE_DIR}/cmake/ImportSFML.cmake)
else ()
    include(FetchContent)
    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.1.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    set(SFML_USE_SYSTEM_DEPS OFF)
    FetchContent_MakeAvailable(SFML)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(freetype PRIVATE harfbuzz)
    endif ()
endif ()

configure_file(${CMAKE_SOURCE_DIR}/src/main.frag ${CMAKE_BINARY_DIR}/main.frag COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/src/main.vert ${CMAKE_BINARY_DIR}/main.vert COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/src/copy.frag ${CMAKE_BINARY_DIR}/copy.frag COPYONLY)

add_executable(main src/main.cpp src/open_sans_semibold.cpp)
target_link_libraries(main PRIVATE
    SFML::Graphics
    SFML::Window
)
