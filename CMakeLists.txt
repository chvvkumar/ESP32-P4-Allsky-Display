# AllSky-Display - pure ESP-IDF project (branch esp-idf).
#
# The legacy Arduino sketch (.ino / .cpp at repo root) is NOT part of this build and is deleted
# in a later phase. Only main/ and components/ are compiled by ESP-IDF.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(allsky_display)
