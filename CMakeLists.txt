cmake_minimum_required(VERSION 4.0)
project(Yume-2kki-Graph)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(GRAPHVIZ_DIR "C:/Program Files/Graphviz")

add_executable(Yume-2kki-Graph main.cpp)

include(FetchContent)

FetchContent_Declare(
        liblcf
        GIT_REPOSITORY https://github.com/EasyRPG/liblcf.git
        GIT_TAG        0.8.1
)

set(BUILD_SHARED_LIBS OFF)

FetchContent_MakeAvailable(liblcf)


file(GLOB GVC_LIBS "${GRAPHVIZ_DIR}/lib/*.lib")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE lcf ${GVC_LIBS})
include_directories("${GRAPHVIZ_DIR}/include/graphviz")