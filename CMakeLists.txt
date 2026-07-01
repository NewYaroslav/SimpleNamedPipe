cmake_minimum_required(VERSION 3.10)
project(SimpleNamedPipe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SIMPLE_NAMED_PIPE_BUILD_EXAMPLES "Build example programs" ON)
option(SIMPLE_NAMED_PIPE_BUILD_STATIC "Build static library from .ipp implementation" ON)
option(SIMPLE_NAMED_PIPE_BUILD_TESTS "Build test programs" OFF)

# Header-only library
add_library(SimpleNamedPipe INTERFACE)
target_include_directories(SimpleNamedPipe INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Optional: static library from NamedPipeServer.ipp
if(SIMPLE_NAMED_PIPE_BUILD_STATIC)
    add_library(SimpleNamedPipeServer STATIC
        src/NamedPipeServer.cpp
    )

    add_library(SimpleNamedPipeClient STATIC
        src/NamedPipeClient.cpp
    )

    target_include_directories(SimpleNamedPipeServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_include_directories(SimpleNamedPipeClient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_definitions(SimpleNamedPipeServer PUBLIC SIMPLE_NAMED_PIPE_STATIC_LIB)
    target_compile_definitions(SimpleNamedPipeClient PUBLIC SIMPLE_NAMED_PIPE_STATIC_LIB)

    set_target_properties(SimpleNamedPipeServer PROPERTIES OUTPUT_NAME snp_server)
    set_target_properties(SimpleNamedPipeClient PROPERTIES OUTPUT_NAME snp_client)
endif()

# Examples
if(SIMPLE_NAMED_PIPE_BUILD_EXAMPLES)
    file(GLOB EXAMPLES ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp)

    foreach(EXAMPLE_FILE ${EXAMPLES})
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_FILE} NAME_WE)
        add_executable(${EXAMPLE_NAME} ${EXAMPLE_FILE})
        target_include_directories(${EXAMPLE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(${EXAMPLE_NAME} PRIVATE SimpleNamedPipe)
        if(SIMPLE_NAMED_PIPE_BUILD_STATIC)
            target_link_libraries(${EXAMPLE_NAME} PRIVATE SimpleNamedPipeServer)
            target_link_libraries(${EXAMPLE_NAME} PRIVATE SimpleNamedPipeClient)
        endif()
    endforeach()
endif()

if(SIMPLE_NAMED_PIPE_BUILD_TESTS AND WIN32)
    add_executable(client_server_test
        tests/client_server_test.cpp
    )
    target_include_directories(client_server_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(client_server_test PRIVATE SimpleNamedPipe)
    if(SIMPLE_NAMED_PIPE_BUILD_STATIC)
        target_link_libraries(client_server_test PRIVATE SimpleNamedPipeServer SimpleNamedPipeClient)
    endif()
endif()
