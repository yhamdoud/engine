cmake_minimum_required(VERSION 3.20)

project(engine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
# TODO: Only enable Tracy in debug builds.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -march=native -DTRACY_ENABLE")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(EXTERN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern")

set(GLAD_DIR "${EXTERN_DIR}/glad")
set(CGLTF_DIR "${EXTERN_DIR}/cgltf")
set(TRACY_DIR "${EXTERN_DIR}/tracy")
set(STB_DIR "${EXTERN_DIR}/stb")
set(FMT_DIR "${EXTERN_DIR}/fmt")
set(IMGUI_DIR "${EXTERN_DIR}/imgui")

file(GLOB SRC_FILES "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.hpp")
add_executable(${PROJECT_NAME} ${SRC_FILES})

# GLFW
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# glad
add_library(glad STATIC "${GLAD_DIR}/src/glad.c")
target_include_directories(glad PRIVATE "${GLAD_DIR}/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${GLAD_DIR}/include")
target_link_libraries(${PROJECT_NAME} PRIVATE glad ${CMAKE_DL_LIBS})

# GLM
find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glm)

# cgtlf
include_directories(${PROJECT_NAME} ${CGLTF_DIR})

# Tracy
add_subdirectory(${TRACY_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${TRACY_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE TracyClient)

# stb
target_include_directories(${PROJECT_NAME} PRIVATE ${STB_DIR})

# fmt
add_subdirectory(${FMT_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE fmt)

# Dear ImGui
file(GLOB IMGUI_SOURCES ${IMGUI_DIR}/*.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp)
add_library(imgui ${IMGUI_SOURCES})
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)