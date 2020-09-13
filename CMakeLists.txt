cmake_minimum_required(VERSION 3.15)
project(CoreMess)

include_directories(/usr/src/linux-headers-4.16.0-041600-generic/include)
include_directories(/usr/src/linux-headers-4.16.0-041600-generic/arch/ia64/include)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
set(SOURCE_FILES
        src/include/config.h
        src/include/new_structures.h
        src/service/module_messy.c src/service/module_messy.h
        src/users/writer.c src/users/reader.c
        src/users/test/multi_readers_writers.c src/users/test/test_conf.h)

add_executable(CoreMess ${SOURCE_FILES})