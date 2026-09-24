idf_component_register(
    SRCS "main.c"
         "uart_transport.c"
         "display.c"
         "encoder.c"
         "sound.c"
         "theme.c"
         "settings.c"
         "astolfo.c"
         "ui.c"
         "ui_widgets.c"
         "app_logic.c"
         "screens/screen_boot.c"
         "screens/screen_menu.c"
         "screens/screen_modules.c"
         "screens/screen_module.c"
         "screens/screen_slave.c"
         "screens/screen_settings.c"
         "screens/screen_about.c"
    INCLUDE_DIRS "." "screens"
    REQUIRES wackos_protocol wackos_gfx wackos_encoder
             esp_driver_gpio esp_driver_uart esp_driver_spi esp_driver_ledc
             esp_lcd esp_timer
)
