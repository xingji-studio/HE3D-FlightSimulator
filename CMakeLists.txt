cmake_minimum_required(VERSION 3.20)

project(HE3D VERSION 0.9 LANGUAGES CXX)

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
    src/he3d_collider.cpp
    src/he3d_gltf.cpp
    src/he3d_convex_builder.cpp
    src/he3d_physics.cpp
)

set(HE3D_PRIVATE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
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
    set(XJ380_SDK_ROOT
        ""
        CACHE PATH
        "Path to one matched XJ380 source/SDK root"
    )

    if(NOT XJ380_SDK_ROOT)
        message(FATAL_ERROR
            "XJ380_SDK_ROOT is required for HE3D_BACKEND=XAPI. "
            "Pass -DXJ380_SDK_ROOT=/path/to/matched/xj380")
    endif()

    if(EXISTS "${XJ380_SDK_ROOT}/user/xapi/include")
        set(HE3D_DEFAULT_XJ380_INCLUDE_DIR "${XJ380_SDK_ROOT}/user/xapi/include")
    else()
        set(HE3D_DEFAULT_XJ380_INCLUDE_DIR "${XJ380_SDK_ROOT}/include")
    endif()

    set(XJ380_XAPI_INCLUDE_DIR
        "${HE3D_DEFAULT_XJ380_INCLUDE_DIR}"
        CACHE PATH
        "Path to matched XJ380 XAPI headers"
    )

    set(XJ380_GUI_RUNTIME_DIR
        "${XJ380_SDK_ROOT}/out/xapi"
        CACHE PATH
        "Path to matched XJ380 XAPI runtime objects"
    )

    if(NOT EXISTS "${XJ380_XAPI_INCLUDE_DIR}/x3api.h")
        message(FATAL_ERROR
            "XJ380 XAPI headers not found at ${XJ380_XAPI_INCLUDE_DIR}. "
            "Set XJ380_XAPI_INCLUDE_DIR to the matched include directory.")
    endif()

    if(NOT EXISTS "${XJ380_GUI_RUNTIME_DIR}")
        message(FATAL_ERROR
            "XJ380 XAPI runtime objects not found at ${XJ380_GUI_RUNTIME_DIR}. "
            "Set XJ380_GUI_RUNTIME_DIR only if it belongs to the same SDK root.")
    endif()

    add_library(he3d_engine_xapi STATIC
        ${HE3D_CORE_SOURCES}
        src/platform/he3d_platform_xapi.cpp
        src/platform/he3d_xapi_runtime.cpp
    )
    add_library(HE3D::Engine ALIAS he3d_engine_xapi)

    target_include_directories(he3d_engine_xapi
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${HE3D_PRIVATE_INCLUDE_DIRS}
            ${XJ380_XAPI_INCLUDE_DIR}
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
        add_library(HE3D::Engine ALIAS he3d_engine_sdl3)

        target_include_directories(he3d_engine_sdl3
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}/include
            PRIVATE
                ${HE3D_PRIVATE_INCLUDE_DIRS}
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
    add_library(HE3D::Engine ALIAS he3d_engine_console)

    target_include_directories(he3d_engine_console
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${HE3D_PRIVATE_INCLUDE_DIRS}
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
    )

    add_executable(he3d_convex_collider_tests
        tests/public_api/he3d_convex_collider_tests.cpp
    )

    target_include_directories(he3d_public_api_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_include_directories(he3d_convex_collider_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_definitions(he3d_public_api_tests
        PRIVATE
            HE3D_BACKEND_CONSOLE
            HE3D_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_compile_definitions(he3d_convex_collider_tests
        PRIVATE
            HE3D_BACKEND_CONSOLE
    )

    target_compile_options(he3d_public_api_tests
        PRIVATE
            $<$<CONFIG:Release>:${HE3D_RELEASE_OPT_FLAG}>
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-g>
    )

    target_compile_options(he3d_convex_collider_tests
        PRIVATE
            $<$<CONFIG:Release>:${HE3D_RELEASE_OPT_FLAG}>
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-g>
    )

    target_link_libraries(he3d_public_api_tests PRIVATE HE3D::Engine)
    target_link_libraries(he3d_convex_collider_tests PRIVATE HE3D::Engine)

    add_test(NAME he3d_public_api_tests COMMAND he3d_public_api_tests)
    add_test(NAME he3d_convex_collider_tests COMMAND he3d_convex_collider_tests)
    add_test(NAME he3d_include_boundaries
        COMMAND
            ${CMAKE_COMMAND}
                -DHE3D_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_include_boundaries.cmake
    )

    add_executable(he3d_console_one_frame_smoke
        tests/integration/he3d_console_one_frame_smoke.cpp
    )
    target_link_libraries(he3d_console_one_frame_smoke PRIVATE HE3D::Engine)
    add_test(NAME he3d_console_one_frame_smoke COMMAND he3d_console_one_frame_smoke)
    set_tests_properties(he3d_console_one_frame_smoke PROPERTIES LABELS Integration)
endif()

if(HE3D_BUILD_INTERNAL_TESTS)
    enable_testing()
    message(STATUS "HE3D internal tests enabled")

    add_executable(he3d_internal_convex_tests
        tests/internal/he3d_internal_convex_tests.cpp
        ${HE3D_CORE_SOURCES}
        src/platform/he3d_platform_console.cpp
    )

    target_include_directories(he3d_internal_convex_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${HE3D_PRIVATE_INCLUDE_DIRS}
    )

    target_compile_definitions(he3d_internal_convex_tests
        PRIVATE
            HE3D_BACKEND_CONSOLE
    )

    target_compile_options(he3d_internal_convex_tests
        PRIVATE
            $<$<CONFIG:Release>:${HE3D_RELEASE_OPT_FLAG}>
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-g>
    )

    add_test(NAME he3d_internal_convex_tests COMMAND he3d_internal_convex_tests)

    add_executable(he3d_internal_physics_tests
        tests/internal/he3d_internal_physics_tests.cpp
        ${HE3D_CORE_SOURCES}
        src/platform/he3d_platform_console.cpp
    )

    target_include_directories(he3d_internal_physics_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${HE3D_PRIVATE_INCLUDE_DIRS}
    )

    target_compile_definitions(he3d_internal_physics_tests
        PRIVATE
            HE3D_BACKEND_CONSOLE
    )

    target_compile_options(he3d_internal_physics_tests
        PRIVATE
            $<$<CONFIG:Release>:${HE3D_RELEASE_OPT_FLAG}>
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-g>
    )

    add_test(NAME he3d_internal_physics_tests COMMAND he3d_internal_physics_tests)
endif()
