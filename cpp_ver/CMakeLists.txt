cmake_minimum_required(VERSION 3.10)
project(unfolder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add Windows Unicode support
add_definitions(-DUNICODE -D_UNICODE)

add_executable(unfolder WIN32 src/main.cpp src/unfolder.rc)
target_link_libraries(unfolder PRIVATE shell32 Shcore)