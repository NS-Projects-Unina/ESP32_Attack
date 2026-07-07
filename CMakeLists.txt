cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(wifi_analyzer)

spiffs_create_partition_image(
    storage
    assets
    FLASH_IN_PROJECT
)
