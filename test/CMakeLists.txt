cmake_minimum_required(VERSION 3.0)
project(my_gameboy_test)

add_executable(my_gameboy_test
    my_gameboy_test.cpp)
target_link_libraries(my_gameboy_test 
    gtest_main
    scg
    body
    cart)