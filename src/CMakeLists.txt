cmake_minimum_required(VERSION 3.0)
project(my_gameboy C)

add_library(scg
    dep/scg/scg.h
    dep/scg/scg.c)
add_library(cart
    src/cart/cart.h
    src/cart/cart.c)
add_library(body
    src/body/cpu.h
    src/body/cpu.c
    src/body/input.h
    src/body/input.c
    src/body/ram.h
    src/body/ram.c
    src/body/screen.h
    src/body/screen.c
    src/body/sound.h
    src/body/sound.c)
add_executable(my_gameboy
    src/world.c)
target_link_libraries(my_gameboy
    scg
    cart
    body)