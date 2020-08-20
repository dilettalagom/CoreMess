cmake_minimum_required(VERSION 3.15)
project(CoreMess)
include_directories(/usr/src/linux-headers-4.16.0-041600-generic/include)
include_directories(/usr/src/linux-headers-4.16.0-041600-generic/arch/ia64/include)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
set(SOURCE_FILES
        src/config.h
        src/module_test.c src/module_test.h
        users/writer.c users/reader.c src/new_structures.h src/test.c)

add_executable(CoreMess ${SOURCE_FILES})


#add_library(CoreMess main.c config.h)