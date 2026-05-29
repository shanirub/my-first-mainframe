cmake_minimum_required(VERSION 3.16)

# relative paths for app and components
set(EXTRA_COMPONENT_DIRS "src" "../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(mcu3-database-controller)