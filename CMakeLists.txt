cmake_minimum_required(VERSION 3.15)

project(gocxx_base VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    gocxx_errors
    GIT_REPOSITORY https://github.com/gocxx/errors.git
    GIT_TAG        v0.0.1
)

FetchContent_MakeAvailable(gocxx_errors)

FetchContent_Declare(
    gocxx_sync
    GIT_REPOSITORY https://github.com/gocxx/sync.git
    GIT_TAG        v0.0.1
)

FetchContent_MakeAvailable(gocxx_sync)

add_library(gocxx_base INTERFACE)

target_include_directories(gocxx_base INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(gocxx_base INTERFACE gocxx_errors gocxx_sync)

install(DIRECTORY include/ DESTINATION include)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/gocxx_base.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/gocxx_base.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gocxx_base.pc
        DESTINATION lib/pkgconfig)

if(GOCXX_ENABLE_TESTS)
    enable_testing()
    FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    file(GLOB BASE_TESTS CONFIGURE_DEPENDS tests/*.cpp)

    add_executable(gocxx_base_tests ${BASE_TESTS})
    target_include_directories(gocxx_base_tests PRIVATE include)
    target_link_libraries(gocxx_base_tests gtest gtest_main gocxx_base)

endif()
