cmake_minimum_required(VERSION 3.0.0)
project(vulkan-tutorial VERSION 0.1.0)

include(CTest)
enable_testing()

find_package(SDL2 REQUIRED)
add_executable(vulkan-tutorial main.cpp)

# Required on Windows
if (TARGET SDL2::SDL2main)
  target_link_libraries(vulkan-tutorial PRIVATE SDL2::SDL2main)
endif()

target_link_libraries(vulkan-tutorial PRIVATE SDL2::SDL2)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
