cmake_minimum_required(VERSION 3.17)

project(vulkan-tutorial)

set (CMAKE_CXX_STANDARD 17)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)

find_package(Vulkan REQUIRED)

if (WIN32)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
endif(WIN32)

add_subdirectory(glfw)

set (
    APPS
    "1.hello-triangle"
)

set(
    MY_HEADERS
    "include/vulkanutils.h"
    "include/queuefamilyindices.h"
)

foreach(APP ${APPS})

    add_executable(
        ${APP} 
        "src/${APP}/${APP}.cpp"
        ${MY_HEADERS}
    )

    set_target_properties(
        ${APP} 
        PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${APP}")

    if (MSVC)
        target_compile_options(${APP} PRIVATE /W4 /WX)
    endif(MSVC)

    target_compile_definitions(
        ${APP}
        PRIVATE 
        GLFW_INCLUDE_VULKAN
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_SILENT_WARNINGS=1
    )
    
    target_include_directories(
        ${APP}
        PRIVATE 
        ${Vulkan_INCLUDE_DIRS}
        "glm/"
        "include/"
    )
    
    target_link_libraries(
        ${APP}
        glfw
        ${Vulkan_LIBRARIES}
    )

endforeach(APP ${APPS})
    

