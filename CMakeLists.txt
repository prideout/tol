cmake_minimum_required(VERSION 2.8.8)
project(tol)

option(EMSCRIPTEN "enable JavaScript output via emcc")

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
pkg_search_module(CURL REQUIRED libcurl)
find_library(OPENGL_LIB OpenGL)

file(GLOB COREC src/*.c)
file(GLOB VENDORC deps/*.c)
file(GLOB SRCFILES ${COREC} ${VENDORC})
file(GLOB JSEXCLUSIONS src/window.c)
file(GLOB JSCPP src/bindings.cpp)

include_directories(
    include
    deps
    ${GLFW_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS})

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-gnu99")
    find_package (X11 REQUIRED)
    add_definitions(
        -O3
        -Wall
        -I../include
        -I../deps)
    set(PLATFORM_LIBS
        ${X11_LIBRARIES}
        Xinerama
        pthread
        dl
        Xrandr
        Xinerama
        Xcursor
        Xi
        Xxf86vm
        GL
        m)
endif()

add_library(parg STATIC ${SRCFILES})

set(EMCCARGS
    -c
    -std=c99
    -O3
    -Wall
    -I../include
    -I../deps)
set(EMCXXARGS
    -O3
    -std=c++11
    -Wall
    -I../include
    -I../deps)
set(EMLINKARGS
    -O3 --memory-init-file 0 --bind
    -s 'MODULARIZE=1'
    -s 'EXPORT_NAME="CreateParg"'
    -s 'NO_FILESYSTEM=1'
    -s 'ALLOW_MEMORY_GROWTH=1'
    -s 'PRECISE_F32=1')

set(JSSRC ${SRCFILES})
list(REMOVE_ITEM JSSRC ${JSEXCLUSIONS})
foreach(SRCFILE ${JSSRC})
    get_filename_component(BASENAME ${SRCFILE} NAME_WE)
    set(JSOBJECT ${BASENAME}.js.o)
    add_custom_command(
        OUTPUT ${JSOBJECT}
        COMMAND emcc -o ${JSOBJECT} ${EMCCARGS} ${SRCFILE}
        DEPENDS ${SRCFILE}
    )
    list(APPEND JSOBJECTS ${JSOBJECT})
endforeach()
foreach(SRCFILE ${JSCPP})
    get_filename_component(BASENAME ${SRCFILE} NAME_WE)
    set(JSOBJECT ${BASENAME}.js.o)
    add_custom_command(
        OUTPUT ${JSOBJECT}
        COMMAND emcc -o ${JSOBJECT} ${EMCXXARGS} ${SRCFILE}
        DEPENDS ${SRCFILE}
    )
    list(APPEND JSOBJECTS ${JSOBJECT})
endforeach()

if(EMSCRIPTEN)
    add_executable(
        app
        src/app.c
        app.glsl
        app.js)
else()
    add_executable(
        app
        src/app.c
        app.glsl)
endif()
add_custom_command(
    OUTPUT app.glsl
    COMMAND cp ../src/app.glsl app.glsl
    DEPENDS src/app.glsl)
target_link_libraries(
    app
    parg
    ${OPENGL_LIB}
    ${GLFW_LIBRARIES}
    ${CURL_LIBRARIES}
    ${PLATFORM_LIBS})
