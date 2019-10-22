cmake_minimum_required(VERSION 3.13)
project(GR66 C)

set(CMAKE_C_STANDARD 99)

add_executable(GR66 main.c socket.c socket.h receive.c receive.h packet_interface.c packet_interface.h)