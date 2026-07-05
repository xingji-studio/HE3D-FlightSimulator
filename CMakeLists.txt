cmake_minimum_required(VERSION 3.20)

project(HE3D LANGUAGES CXX)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(HE3D_BACKEND "XAPI" CACHE STRING "HE3D backend to build: XAPI, SDL3, or CONSOLE")
set_property(CACHE HE3D_BACKEND PROPERTY STRINGS XAPI SDL3 CONSOLE)
string(TOUPPER "${HE3D_BACKEND}" HE3D_BACKEND)
if(NOT HE3D_BACKEND STREQUAL "XAPI" AND NOT HE3D_BACKEND STREQUAL "SDL3" AND NOT HE3D_BACKEND STREQUAL "CONSOLE")
    message(FATAL_ERROR "HE3D_BACKEND must be XAPI, SDL3, or CONSOLE")
endif()
message(STATUS "HE3D backend: ${HE3D_BACKEND}")

set(HE3D_EXAMPLE "TriangleTest" CACHE STRING "HE3D example to build: TriangleTest, AirplaneTest, PhysicsTest, or FlightSimulator")
set_property(CACHE HE3D_EXAMPLE PROPERTY STRINGS TriangleTest AirplaneTest PhysicsTest FlightSimulator)
if(NOT HE3D_EXAMPLE STREQUAL "FlightSimulator" AND
   NOT HE3D_EXAMPLE STREQUAL "AirplaneTest" AND
   NOT HE3D_EXAMPLE STREQUAL "PhysicsTest" AND
   NOT HE3D_EXAMPLE STREQUAL "TriangleTest")
    message(FATAL_ERROR "HE3D_EXAMPLE must be TriangleTest, AirplaneTest, PhysicsTest, or FlightSimulator")
endif()
message(STATUS "HE3D example: ${HE3D_EXAMPLE}")

set(HE3D_RELEASE_OPT_FLAG -O2)
message(STATUS "HE3D Release optimization: ${HE3D_RELEASE_OPT_FLAG}")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HE3D_BUILD_TESTS "Build HE3D public API tests" OFF)
option(HE3D_BUILD_INTERNAL_TESTS "Build HE3D internal implementation tests" OFF)

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

set(HE3D_TRIANGLE_TEST_SOURCES
    examples/TriangleTest/src/main.cpp
)

set(HE3D_AIRPLANE_TEST_SOURCES
    examples/AirplaneTest/src/main.cpp
)

set(HE3D_PHYSICS_TEST_SOURCES
    examples/PhysicsTest/src/main.cpp
)

if(HE3D_EXAMPLE STREQUAL "FlightSimulator")
    set(HE3D_APP_SOURCES ${HE3D_FLIGHT_SOURCES})
    set(HE3D_APP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples/FlightSimulator/src)
    set(HE3D_APP_NAME he3d_flight_simulator)
elseif(HE3D_EXAMPLE STREQUAL "AirplaneTest")
    set(HE3D_APP_SOURCES ${HE3D_AIRPLANE_TEST_SOURCES})
    set(HE3D_APP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples/AirplaneTest/src)
    set(HE3D_APP_NAME he3d_airplane_test)
elseif(HE3D_EXAMPLE STREQUAL "PhysicsTest")
    set(HE3D_APP_SOURCES ${HE3D_PHYSICS_TEST_SOURCES})
    set(HE3D_APP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples/PhysicsTest/src)
    set(HE3D_APP_NAME he3d_physics_test)
elseif(HE3D_EXAMPLE STREQUAL "TriangleTest")
    set(HE3D_APP_SOURCES ${HE3D_TRIANGLE_TEST_SOURCES})
    set(HE3D_APP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples/TriangleTest/src)
    set(HE3D_APP_NAME he3d_triangle_test)
endif()

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

function(he3d_copy_airplane_assets target_name)
    set(HE3D_AIRPLANE_MODEL
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/AirplaneTest/assets/model.obj")
    if(EXISTS "${HE3D_AIRPLANE_MODEL}")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${HE3D_AIRPLANE_MODEL}
                $<TARGET_FILE_DIR:${target_name}>/model.obj
        )
    else()
        message(WARNING
            "AirplaneTest model.obj is not present; the example will build but "
            "needs model.obj next to the executable at runtime.")
    endif()
endfunction()

function(he3d_copy_example_assets target_name)
    if(HE3D_EXAMPLE STREQUAL "FlightSimulator")
        he3d_copy_flight_assets(${target_name})
    elseif(HE3D_EXAMPLE STREQUAL "AirplaneTest")
        he3d_copy_airplane_assets(${target_name})
    endif()
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
            $<$<CONFIG:Release>:${HE3D_RELEASE_OPT_FLAG}>
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

    set(XJ380_GUI_RUNTIME_DIR
        ""
        CACHE PATH
        "Path to prebuilt XJ380 GUI runtime objects"
    )
    if(NOT XJ380_GUI_RUNTIME_DIR)
        foreach(candidate
                "${XJ380_SDK_ROOT}/obj-gui"
                "${CMAKE_CURRENT_SOURCE_DIR}/../XJ380/out/xapi")
            if(EXISTS "${candidate}")
                set(XJ380_GUI_RUNTIME_DIR "${candidate}")
                break()
            endif()
        endforeach()
    endif()

    if(NOT EXISTS "${XJ380_GUI_RUNTIME_DIR}")
        message(FATAL_ERROR
            "XJ380 GUI runtime objects not found. Checked: "
            "${XJ380_SDK_ROOT}/obj-gui and "
            "${CMAKE_CURRENT_SOURCE_DIR}/../XJ380/out/xapi. "
            "Set XJ380_GUI_RUNTIME_DIR to override."
        )
    endif()

    file(GLOB_RECURSE XJ380_GUI_OBJECTS CONFIGURE_DEPENDS
        "${XJ380_GUI_RUNTIME_DIR}/*.o"
    )

    file(GLOB_RECURSE XJ380_GUI_ARCHIVES CONFIGURE_DEPENDS
        "${XJ380_GUI_RUNTIME_DIR}/*.a"
    )
    list(FILTER XJ380_GUI_OBJECTS EXCLUDE REGEX ".*/constart\.cpp\.o$")

    add_executable(he3d_xapi_app
        ${HE3D_APP_SOURCES}
    )

    set_target_properties(he3d_xapi_app
        PROPERTIES
            OUTPUT_NAME "${HE3D_APP_NAME}.elf")

    target_link_libraries(he3d_xapi_app
        PRIVATE
            he3d_engine_xapi
            ${XJ380_GUI_OBJECTS}
            ${XJ380_GUI_ARCHIVES}
    )

    target_include_directories(he3d_xapi_app
        PRIVATE
            ${HE3D_APP_INCLUDE_DIR}
    )

    target_compile_options(he3d_xapi_app
        PRIVATE
            ${HE3D_XJ380_COMPILE_OPTIONS}
            $<$<CONFIG:Release>:${HE3D_RELEASE_OPT_FLAG}>
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-g>
    )

    target_compile_definitions(he3d_xapi_app
        PRIVATE
            __XJ380_OS__
    )

    target_link_options(he3d_xapi_app
        PRIVATE
            -nostdlib
            -no-pie
            -Wl,-Ttext=0x200000
    )

    he3d_copy_example_assets(he3d_xapi_app)
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

        target_compile_definitions(he3d_engine_sdl3
            PUBLIC
                HE3D_BACKEND_SDL3
        )

        target_compile_options(he3d_engine_sdl3
            PRIVATE
                $<$<CONFIG:Release>:${HE3D_RELEASE_OPT_FLAG}>
                $<$<CONFIG:Debug>:-O0>
                $<$<CONFIG:Debug>:-g>
        )

        target_link_libraries(he3d_engine_sdl3
            PUBLIC
                PkgConfig::SDL3
        )

        add_executable(he3d_sdl3_app
            ${HE3D_APP_SOURCES}
        )

        set_target_properties(he3d_sdl3_app
            PROPERTIES
                OUTPUT_NAME "${HE3D_APP_NAME}_sdl3")

        target_include_directories(he3d_sdl3_app
            PRIVATE
                ${HE3D_APP_INCLUDE_DIR}
        )

        target_compile_options(he3d_sdl3_app
            PRIVATE
                $<$<CONFIG:Release>:${HE3D_RELEASE_OPT_FLAG}>
                $<$<CONFIG:Debug>:-O0>
                $<$<CONFIG:Debug>:-g>
        )

        target_link_libraries(he3d_sdl3_app
            PRIVATE
                he3d_engine_sdl3
        )

        he3d_copy_example_assets(he3d_sdl3_app)
    else()
        message(FATAL_ERROR "SDL3 was not found; install SDL3 development files or choose HE3D_BACKEND=XAPI/CONSOLE")
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

    target_compile_definitions(he3d_engine_console
        PUBLIC
            HE3D_BACKEND_CONSOLE
    )

    target_compile_options(he3d_engine_console
            PRIVATE
                $<$<CONFIG:Release>:${HE3D_RELEASE_OPT_FLAG}>
                $<$<CONFIG:Debug>:-O0>
                $<$<CONFIG:Debug>:-g>
        )

    add_executable(he3d_console_app
        ${HE3D_APP_SOURCES}
    )

    set_target_properties(he3d_console_app
        PROPERTIES
            OUTPUT_NAME "${HE3D_APP_NAME}_console")

    target_include_directories(he3d_console_app
        PRIVATE
            ${HE3D_APP_INCLUDE_DIR}
    )

    target_compile_options(he3d_console_app
            PRIVATE
                $<$<CONFIG:Release>:${HE3D_RELEASE_OPT_FLAG}>
                $<$<CONFIG:Debug>:-O0>
                $<$<CONFIG:Debug>:-g>
        )

    target_link_libraries(he3d_console_app
        PRIVATE
            he3d_engine_console
    )

    he3d_copy_example_assets(he3d_console_app)
endif()

if(HE3D_BUILD_TESTS)
    enable_testing()

    add_executable(he3d_public_api_tests
        tests/public_api/he3d_public_api_tests.cpp
        ${HE3D_CORE_SOURCES}
        src/platform/he3d_platform_console.cpp
    )

    target_include_directories(he3d_public_api_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_definitions(he3d_public_api_tests
        PRIVATE
            HE3D_BACKEND_CONSOLE
    )

    target_compile_options(he3d_public_api_tests
        PRIVATE
            $<$<CONFIG:Release>:${HE3D_RELEASE_OPT_FLAG}>
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-g>
    )

    add_test(NAME he3d_public_api_tests COMMAND he3d_public_api_tests)
    add_test(NAME he3d_include_boundaries
        COMMAND
            ${CMAKE_COMMAND}
                -DHE3D_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_include_boundaries.cmake
    )
endif()

if(HE3D_BUILD_INTERNAL_TESTS)
    enable_testing()
    message(STATUS "HE3D internal tests enabled")
endif()
