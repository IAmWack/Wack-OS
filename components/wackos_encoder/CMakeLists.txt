# Декодер энкодера: чистая логика без железа, общая для master (своя
# ручка) и slave (ручка, подключённая к нему на время отладки).
idf_component_register(
    SRCS "wackos_encoder.c"
    INCLUDE_DIRS "include"
)
