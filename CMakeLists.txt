cmake_minimum_required(VERSION 3.16)
project(myvkhl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(myvkhl src/main.cpp)

target_include_directories(myvkhl PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(myvkhl PRIVATE ${Vulkan_LIBRARIES} glfw)

