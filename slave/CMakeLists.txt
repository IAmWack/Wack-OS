cmake_minimum_required(VERSION 3.16)

# Тот же общий компонент протокола, что и у master — см. master/CMakeLists.txt
# и docs/ARCHITECTURE.md. Меняется от платы к плате только main/.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wackos_slave)
