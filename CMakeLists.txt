cmake_minimum_required(VERSION 3.20)

project(engine)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED TRUE)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(EXTERN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern")
set(GLAD_DIR "${EXTERN_DIR}/glad")

file(GLOB SRC_FILES "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.hpp")
add_executable(${PROJECT_NAME} ${SRC_FILES})

# GLFW
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# glad
add_library(glad STATIC "${GLAD_DIR}/src/glad.c")
target_include_directories(glad PRIVATE "${GLAD_DIR}/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${GLAD_DIR}/include")
target_link_libraries(${PROJECT_NAME} PRIVATE glad "${CMAKE_DL_LIBS}")

# GLM
find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glm)