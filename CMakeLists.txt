cmake_minimum_required(VERSION 3.20)

project(HE3D LANGUAGES CXX)

set(HE3D_BACKEND "XAPI" CACHE STRING "HE3D backend to build: XAPI, SDL3, or CONSOLE")
set_property(CACHE HE3D_BACKEND PROPERTY STRINGS XAPI SDL3 CONSOLE)
string(TOUPPER "${HE3D_BACKEND}" HE3D_BACKEND)
if(NOT HE3D_BACKEND STREQUAL "XAPI" AND NOT HE3D_BACKEND STREQUAL "SDL3" AND NOT HE3D_BACKEND STREQUAL "CONSOLE")
    message(FATAL_ERROR "HE3D_BACKEND must be XAPI, SDL3, or CONSOLE")
endif()
message(STATUS "HE3D backend: ${HE3D_BACKEND}")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    string(REGEX REPLACE "(^| )-O[0-3sSzZgfast]+($| )" " " CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()

set(HE3D_CORE_SOURCES
    src/he3d.cpp
)

set(HE3D_FLIGHT_SOURCES
    examples/FlightSimulator/src/flight_sim.cpp
    examples/FlightSimulator/src/main.cpp
)

set(HE3D_XJ380_COMPILE_OPTIONS
    -ffreestanding
    -fno-builtin
    -m64
    -fno-stack-protector
    -fno-exceptions
    -fshort-wchar
    -nostdinc
)

function(he3d_copy_flight_assets target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/FlightSimulator/assets/biplane.obj
            $<TARGET_FILE_DIR:${target_name}>/biplane.obj
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/FlightSimulator/assets/biplane.bmp
            $<TARGET_FILE_DIR:${target_name}>/biplane.bmp
    )
endfunction()

if(HE3D_BACKEND STREQUAL "XAPI")
    add_library(he3d_engine_xapi STATIC
        ${HE3D_CORE_SOURCES}
        src/platform/he3d_platform_xapi.cpp
    )

    target_include_directories(he3d_engine_xapi
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/xj380/include
    )

    target_compile_options(he3d_engine_xapi
        PRIVATE
            ${HE3D_XJ380_COMPILE_OPTIONS}
            $<$<CONFIG:Release>:-O2>
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-g>
    )

    target_compile_definitions(he3d_engine_xapi
        PUBLIC
            __XJ380_OS__
    )

    set(XJ380_SDK_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/../XXCC-suite"
        CACHE PATH
        "Path to the XJ380 development suite"
    )

    if(NOT EXISTS "${XJ380_SDK_ROOT}/obj-gui")
        message(FATAL_ERROR "XJ380 GUI runtime objects not found: ${XJ380_SDK_ROOT}/obj-gui")
    endif()

    file(GLOB_RECURSE XJ380_GUI_OBJECTS CONFIGURE_DEPENDS
        "${XJ380_SDK_ROOT}/obj-gui/*.o"
    )

    file(GLOB_RECURSE XJ380_GUI_ARCHIVES CONFIGURE_DEPENDS
        "${XJ380_SDK_ROOT}/obj-gui/*.a"
    )

    add_executable(he3d_flight_simulator
        ${HE3D_FLIGHT_SOURCES}
    )

    set_target_properties(he3d_flight_simulator
        PROPERTIES
            OUTPUT_NAME "he3d_flight_simulator.elf")

    target_link_libraries(he3d_flight_simulator
        PRIVATE
            he3d_engine_xapi
            ${XJ380_GUI_OBJECTS}
            ${XJ380_GUI_ARCHIVES}
    )

    target_include_directories(he3d_flight_simulator
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/FlightSimulator/src
    )

    target_compile_options(he3d_flight_simulator
        PRIVATE
            ${HE3D_XJ380_COMPILE_OPTIONS}
            $<$<CONFIG:Release>:-O2>
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-g>
    )

    target_compile_definitions(he3d_flight_simulator
        PRIVATE
            __XJ380_OS__
    )

    target_link_options(he3d_flight_simulator
        PRIVATE
            -nostdlib
            -no-pie
            -Wl,-Ttext=0x200000
    )

    he3d_copy_flight_assets(he3d_flight_simulator)
endif()

if(HE3D_BACKEND STREQUAL "SDL3")
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL3 QUIET IMPORTED_TARGET sdl3)
    endif()

    if(SDL3_FOUND)
        add_library(he3d_engine_sdl3 STATIC
            ${HE3D_CORE_SOURCES}
            src/platform/he3d_platform_sdl3.cpp
        )

        target_include_directories(he3d_engine_sdl3
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

        target_compile_options(he3d_engine_sdl3
            PRIVATE
                $<$<CONFIG:Release>:-O2>
                $<$<CONFIG:Debug>:-O0>
                $<$<CONFIG:Debug>:-g>
        )

        target_link_libraries(he3d_engine_sdl3
            PUBLIC
                PkgConfig::SDL3
        )

        add_executable(he3d_flight_simulator_sdl3
            ${HE3D_FLIGHT_SOURCES}
        )

        target_include_directories(he3d_flight_simulator_sdl3
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/FlightSimulator/src
        )

        target_compile_options(he3d_flight_simulator_sdl3
            PRIVATE
                $<$<CONFIG:Release>:-O2>
                $<$<CONFIG:Debug>:-O0>
                $<$<CONFIG:Debug>:-g>
        )

        target_link_libraries(he3d_flight_simulator_sdl3
            PRIVATE
                he3d_engine_sdl3
        )

        he3d_copy_flight_assets(he3d_flight_simulator_sdl3)
    else()
        message(WARNING "SDL3 was not found; he3d_flight_simulator_sdl3 will not be built")
    endif()
endif()

if(HE3D_BACKEND STREQUAL "CONSOLE")
    add_library(he3d_engine_console STATIC
        ${HE3D_CORE_SOURCES}
        src/platform/he3d_platform_console.cpp
    )

    target_include_directories(he3d_engine_console
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_options(he3d_engine_console
        PRIVATE
            $<$<CONFIG:Release>:-O2>
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-g>
    )

    add_executable(he3d_flight_simulator_console
        ${HE3D_FLIGHT_SOURCES}
    )

    target_include_directories(he3d_flight_simulator_console
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/FlightSimulator/src
    )

    target_compile_options(he3d_flight_simulator_console
        PRIVATE
            $<$<CONFIG:Release>:-O2>
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-g>
    )

    target_link_libraries(he3d_flight_simulator_console
        PRIVATE
            he3d_engine_console
    )

    he3d_copy_flight_assets(he3d_flight_simulator_console)
endif()
