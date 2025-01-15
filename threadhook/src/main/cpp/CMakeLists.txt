cmake_minimum_required(VERSION 3.6)
project("threadhook")

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  ${WARNING_FLAGS}  -std=gnu11 -pthread -marm ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS} -std=c++11 -pthread ")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--hash-style=gun")


if (${ANDROID_ABI} STREQUAL "x86")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS} -std=c++11 -pthread  -D__X86__")
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS} -std=c++11 -pthread -D__ARM__ ")
endif ()


add_library(
        ${CMAKE_PROJECT_NAME}
        SHARED
        thread_hook.h
        thread_hook.cpp
        thread_compressor.h
        thread_stack_compressed.cpp
        hook.cpp
)

find_package(shadowhook REQUIRED CONFIG)

# add_library(mylib SHARED mylib.c)


find_library(
        log-lib
        log)

target_link_libraries(
        ${CMAKE_PROJECT_NAME}
        android
        ${log-lib}
        shadowhook::shadowhook)


