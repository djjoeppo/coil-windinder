# The following lines of boilerplate have to be in your project's
# CMakeLists.txt or at least before project() is called.
cmake_minimum_required(VERSION 3.16)

# Explicitly add "conponents" as a custom component directory because of the spelling
set(EXTRA_COMPONENT_DIRS "conponents")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(windmachine_esp32)
