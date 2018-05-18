cmake_minimum_required(VERSION 3.10)
project(pc_2018_01 C)

set(CMAKE_C_STANDARD 11)

include_directories(include)
include_directories(parsers/include)
include_directories(parsers/map-master/src)

add_executable(pc_2018_01
        include/buffer.h
        include/styles.h
        parsers/include/http_request_parser.h
        parsers/include/http_response_parser.h
        parsers/map-master/src/map.c
        parsers/map-master/src/map.h
        parsers/http_request_parser.c
        parsers/http_response_parser.c
        parsers/media_types_parser.c
        buffer.c
        comunication_between_proceses.c
        main.c
        serverTCP.c)
