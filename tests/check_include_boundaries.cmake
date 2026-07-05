if(NOT DEFINED HE3D_SOURCE_DIR)
    message(FATAL_ERROR "HE3D_SOURCE_DIR is required")
endif()

set(check_roots
    examples
    tests/public_api
)

set(source_patterns
    "*.c"
    "*.cc"
    "*.cpp"
    "*.cxx"
    "*.h"
    "*.hh"
    "*.hpp"
    "*.hxx"
)

set(files)
foreach(root IN LISTS check_roots)
    foreach(pattern IN LISTS source_patterns)
        file(GLOB_RECURSE matched
            RELATIVE "${HE3D_SOURCE_DIR}"
            "${HE3D_SOURCE_DIR}/${root}/${pattern}"
        )
        list(APPEND files ${matched})
    endforeach()
endforeach()

list(REMOVE_DUPLICATES files)
list(SORT files)

set(failures)
foreach(path IN LISTS files)
    file(READ "${HE3D_SOURCE_DIR}/${path}" content)
    string(REPLACE "\r\n" "\n" content "${content}")
    string(REPLACE "\r" "\n" content "${content}")
    string(REPLACE "\n" ";" lines "${content}")

    set(line_number 0)
    foreach(line IN LISTS lines)
        math(EXPR line_number "${line_number} + 1")
        if(line MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
            set(include_path "${CMAKE_MATCH_1}")
            if(include_path MATCHES "^\\.\\./" OR
               include_path MATCHES "^src/" OR
               include_path MATCHES "/src/" OR
               include_path MATCHES "he3d\\.cpp$")
                list(APPEND failures "${path}:${line_number}: forbidden include '${include_path}'")
            endif()
        endif()
    endforeach()
endforeach()

if(failures)
    string(REPLACE ";" "\n" failure_text "${failures}")
    message(FATAL_ERROR "Include boundary violations:\n${failure_text}")
endif()

message(STATUS "HE3D include boundary check passed")
