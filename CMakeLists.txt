cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lilygo-eink)

target_compile_options(lilygo-eink.elf PRIVATE -Werror)
