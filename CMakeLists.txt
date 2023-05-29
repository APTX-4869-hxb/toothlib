cmake_minimum_required(VERSION 3.16)

project(toothlib)

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# if(NOT CMAKE_BUILD_TYPE)
# set(CMAKE_BUILD_TYPE Release)
# endif()
set(CMAKE_CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

if(NOT USER_ID OR NOT USER_TOKEN OR NOT SERVER_URL OR NOT FILE_SERVER_URL)
    message(
        FATAL_ERROR
        "Credential needed. command: cmake .. -DUSER_ID=ID -DUSER_TOKEN=TOKEN -DSERVER_URL=URL -DFILE_SERVER_URL=URL")
endif()

message("USER_ID=${USER_ID}")
message("USER_TOKEN=${USER_TOKEN}")
message("SERVER_URL=${SERVER_URL}")
message("FILE_SERVER_URL=${FILE_SERVER_URL}")

if(USER_TOKEN_LEN LESS 9)
    message(FATAL_ERROR "Credential format is not correct!")
endif()

add_definitions(-DUSER_ID="${USER_ID}")
add_definitions(-DUSER_TOKEN="${USER_TOKEN}")
add_definitions(-DSERVER_URL="${SERVER_URL}")
add_definitions(-DFILE_SERVER_URL="${FILE_SERVER_URL}")

# CMake module path
list(PREPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake/
    ${CMAKE_CURRENT_LIST_DIR}/cmake/igl
    ${CMAKE_CURRENT_LIST_DIR}/cmake/find
    ${CMAKE_CURRENT_LIST_DIR}/cmake/recipes/external
)

set_property(GLOBAL PROPERTY __igl_module_path ${CMAKE_MODULE_PATH})

include(igl_add_library)
include(igl_add_test)
include(igl_add_tutorial)
include(igl_copy_dll)
include(igl_include)
include(igl_install)
include(igl_target_sources)

# Libigl
include(libigl)
include(FetchContent)

# Enable the target igl::glfw
igl_include(glfw)
igl_include(imgui)

FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 0817715923c9705e68994eb52ef9df3f6845beba) # The commit hash for 1.10.x. Replace with the latest from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)

# Add your project files
file(GLOB SRC_FILES *.cpp)

include_directories(include)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)
target_link_libraries(${PROJECT_NAME} PUBLIC igl::glfw)
target_link_libraries(${PROJECT_NAME} PUBLIC igl::imgui)
