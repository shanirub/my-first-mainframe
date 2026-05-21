cmake_minimum_required(VERSION 3.16)

# Tell IDF the app component lives in src/ rather than the default main/
set(EXTRA_COMPONENT_DIRS "src")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(mcu3-database-controller)