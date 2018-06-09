cmake_minimum_required(VERSION 3.10)
project(v2 C)

set(CMAKE_C_STANDARD 11)

find_package (Threads)

include_directories(.)

add_executable(v2
        buffer.c
        buffer.h
        http.c
        http.h
        httpnio.c
        httpnio.h
        main.c
        netutils.c
        netutils.h
        parser.c
        parser.h
        parser_utils.c
        parser_utils.h
        request.c
        request.h
        selector.c
        selector.h
        stm.c
        stm.h
        sctp/sctp_integration.c
        sctp/sctp_integration.h
        http_codes.h
        parameters.c
        parameters.h
        media_types.c
        media_types.h
        )
target_link_libraries(v2 -L/usr/local/lib)
target_link_libraries(v2 -lsctp)
target_link_libraries (v2 ${CMAKE_THREAD_LIBS_INIT})