find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    set(FILE_COLLECTOR_GTEST_SOURCE_DIR "" CACHE PATH "Existing local googletest source checkout")
    if(FILE_COLLECTOR_GTEST_SOURCE_DIR)
        add_subdirectory("${FILE_COLLECTOR_GTEST_SOURCE_DIR}"
                         "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(googletest
            URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
        )
        FetchContent_MakeAvailable(googletest)
    endif()
endif()

add_executable(file_collector_tests
    collector_tests.cpp
    chunk_tests.cpp
    boundary_tests.cpp
    concurrency_tests.cpp
    lifecycle_tests.cpp
    snapshot_tests.cpp
    exception_tests.cpp
    thread_pool_tests.cpp
    stress_tests.cpp
    performance_tests.cpp
)
target_include_directories(file_collector_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(file_collector_tests PRIVATE file_collector GTest::gtest_main Threads::Threads)

include(GoogleTest)
gtest_discover_tests(file_collector_tests
    DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT 60
    PROPERTIES TIMEOUT 120
)

