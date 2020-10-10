cmake_minimum_required(VERSION 3.15)
project(CoreMess)

include_directories(/usr/src/linux-headers-4.16.0-041600-generic/include)
include_directories(/usr/src/linux-headers-4.16.0-041600-generic/arch/ia64/include)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
set(SOURCE_FILES
        src/include/config.h
        src/include/new_structures.h
        src/service/module_messy.h src/service/module_messy.c
        src/users/writer.c src/users/reader.c
        src/users/test/test_conf.h
        src/users/test/1_write_read_test.c src/users/test/2_defwrite_read_test.c src/users/test/3_defread_write_test.c src/users/test/4_rekove_messages_test.c src/users/test/5_flushing_defworks_test.c src/users/test/6_multi_readers_writers_test.c)

add_executable(CoreMess ${SOURCE_FILES})