cmake_minimum_required(VERSION 3.16)

# Протокол — общий компонент для master и slave (../components/wackos_protocol).
# Это и есть та граница, о которой docs/ARCHITECTURE.md: слой протокола
# переиспользуется без изменений, а всё железо — только в main/.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wackos_master)
