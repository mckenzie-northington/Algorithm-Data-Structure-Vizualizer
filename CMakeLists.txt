cmake_minimum_required(VERSION 3.15)
project(AlgorithmVisualizer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Allow older CMake syntax in dependencies (needed for CMake 4.x)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)

# Don't build raylib's examples/games, just the library
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# --- Our executable ---
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/UI.cpp
    src/Playback.cpp
    src/SortingScene.cpp
    src/StackQueueScene.cpp
    src/LinkedListScene.cpp
    src/BSTScene.cpp
    src/HeapScene.cpp
    src/HashTableScene.cpp
    src/GraphScene.cpp
    src/PathfindingScene.cpp
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /W3)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

# macOS needs some extra system frameworks for raylib to link correctly
if (APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
endif()

# Windows: build as a GUI app so double-clicking it doesn't open a console window.
# (Keeps the normal main() entry point.)
if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    if (MSVC)
        target_link_options(${PROJECT_NAME} PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()
