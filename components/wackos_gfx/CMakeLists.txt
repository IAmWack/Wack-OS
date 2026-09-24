# Слой рисования: чистый C без зависимостей от железа — тот же код
# собирается и в прошивке master, и в симуляторе sim/ на ноутбуке.
idf_component_register(
    SRCS "wackos_gfx.c" "wackos_font.c" "wackos_font_data.c"
    INCLUDE_DIRS "include"
)
