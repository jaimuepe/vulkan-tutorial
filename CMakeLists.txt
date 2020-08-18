cmake_minimum_required(VERSION 3.17)

project(vulkan-tutorial)

set (CMAKE_CXX_STANDARD 17)

### update all git submodules
#############################

find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    RESULT_VARIABLE GIT_SUBMOD_RESULT)
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
    endif()
endif()

### prepare & build projects
############################

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)

find_package(Vulkan REQUIRED)

set (LIBS
    glfw
    ${Vulkan_LIBRARIES}
)

if (WIN32)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
    set(LIBS
        ${LIBS}
        pathcch
    )
endif(WIN32)

add_subdirectory(glfw)

set (
    APPS
    "1.hello-triangle"
)

set(
    MY_HEADERS
    "include/filesystemutils.h"
    "include/queuefamilyindices.h"
    "include/swapchainsupportdetails.h"
    "include/vulkanutils.h"
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
        # target_compile_options(${APP} PRIVATE /W4 /WX)
        target_compile_options(${APP} PRIVATE /W4)
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
        ${LIBS}
    )

endforeach(APP ${APPS})

### Compile the shaders into .spv 
##################################

file(
    GLOB GLSL_SHADERS
    "shaders/*.frag"
    "shaders/*.vert"
)

find_program(GLSLC NAMES glslc)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found!")
endif()

set (BIN_SPV_SHADERS)

foreach (GLSL_SHADER ${GLSL_SHADERS})

    get_filename_component(
        SHADER_NAME
        ${GLSL_SHADER}
        NAME
    )
    
    set (SPV_SHADER_NAME ${SHADER_NAME}.spv)
    set (SPV_SHADER ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SPV_SHADER_NAME})
    
    add_custom_command(
        OUTPUT ${SPV_SHADER}
        MAIN_DEPENDENCY ${GLSL_SHADER}
        COMMAND ${GLSLC} -Werror ${GLSL_SHADER} -o ${SPV_SHADER}
        VERBATIM
        COMMENT "Compiling shader ${SHADER_NAME}..."
    )

    list(APPEND BIN_SPV_SHADERS ${SPV_SHADER})

endforeach(GLSL_SHADER)

add_custom_target(
    compile-shaders ALL
    DEPENDS ${BIN_SPV_SHADERS}
)

### Copy the compiled .spv shaders 
### into each target's bin directory
##################################

set(TARGET_SPV_SHADERS)

foreach (SPV_SHADER ${BIN_SPV_SHADERS})

    get_filename_component(
        SPV_SHADER_NAME
        ${SPV_SHADER}
        NAME
    )
    
    foreach(APP ${APPS})
        
        set(TARGET_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${APP})
        set(TARGET_SPV_SHADER ${TARGET_OUTPUT_DIR}/${SPV_SHADER_NAME})

        add_custom_command(
            OUTPUT ${TARGET_SPV_SHADER}
            MAIN_DEPENDENCY ${SPV_SHADER}
            COMMAND ${CMAKE_COMMAND} -E copy 
            ${SPV_SHADER} 
            ${TARGET_OUTPUT_DIR}
            COMMENT "Copying shader ${SPV_SHADER_NAME} to ${TARGET_OUTPUT_DIR} ..."
        )
        LIST(APPEND TARGET_SPV_SHADERS ${TARGET_SPV_SHADER})

    endforeach(APP)

endforeach(SPV_SHADER)

add_custom_target(
    copy-shaders ALL
    DEPENDS ${TARGET_SPV_SHADERS}
)