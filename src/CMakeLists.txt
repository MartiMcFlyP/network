cmake_minimum_required(VERSION 3.13)
project(src C)

set(CMAKE_C_STANDARD 99)

add_executable(src main.c packet_implement.h server.c receive.h)