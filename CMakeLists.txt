cmake_minimum_required(VERSION 4.0)
project(COMP7005Proj1 C)

set(CMAKE_C_STANDARD 17)

add_executable(COMP7005Proj1
        client.c
        client.h
        server.c
        server.h
        proxy.c
        proxy.h
        server.h
        protocol.c
        protocol.h)
