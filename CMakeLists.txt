cmake_minimum_required(VERSION 3.25)
project(FileByChunks VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FILE_COLLECTOR_BUILD_TESTS "Build GoogleTest suite" ON)
option(FILE_COLLECTOR_ASAN_UBSAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(FILE_COLLECTOR_TSAN "Enable ThreadSanitizer" OFF)

if(FILE_COLLECTOR_ASAN_UBSAN AND FILE_COLLECTOR_TSAN)
    message(FATAL_ERROR "Use ASan/UBSan and TSan in separate builds")
endif()

if(FILE_COLLECTOR_ASAN_UBSAN OR FILE_COLLECTOR_TSAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "The selected sanitizers require Clang or GCC")
    endif()
    if(FILE_COLLECTOR_TSAN)
        set(SANITIZER_FLAGS -fsanitize=thread -fno-omit-frame-pointer -g)
    else()
        set(SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    endif()
    add_compile_options(${SANITIZER_FLAGS})
    add_link_options(${SANITIZER_FLAGS})
endif()

find_package(Threads REQUIRED)

add_library(file_collector SHARED
    src/file_collector.cpp
    src/file_state.cpp
    src/thread_pool.cpp
)
target_include_directories(file_collector PUBLIC include PRIVATE src)
target_link_libraries(file_collector PRIVATE Threads::Threads)
target_compile_features(file_collector PUBLIC cxx_std_20)
set_target_properties(file_collector PROPERTIES
    CXX_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN OFF
)

if(FILE_COLLECTOR_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

