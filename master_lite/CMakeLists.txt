cmake_minimum_required(VERSION 3.16)

# master_lite — урезанная ветка master/: автономный "мультитул" без связи
# со slave (см. master/main/board_pins.h для комментариев по железу —
# плата та же, Waveshare ESP32-S3-LCD-1.47).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wackos_lite)
