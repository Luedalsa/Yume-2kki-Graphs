cmake_minimum_required(VERSION 4.0)
project(Yume-2kki-Graph)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

add_executable(Yume-2kki-Graph main.cpp)

include(FetchContent)

FetchContent_Declare(
        liblcf
        GIT_REPOSITORY https://github.com/EasyRPG/liblcf.git
        GIT_TAG        0.8.1
)

set(BUILD_SHARED_LIBS OFF)

FetchContent_MakeAvailable(liblcf)

target_link_libraries(Yume-2kki-Graph PRIVATE lcf)
