idf_component_register(
    SRCS "main.c" "uart_transport.c" "dispatcher.c"
         "modules/module_radio.c" "modules/module_hid.c" "modules/module_sensor.c"
         "modules/module_encoder.c"
    INCLUDE_DIRS "."
    REQUIRES wackos_protocol wackos_encoder esp_driver_uart esp_timer esp_driver_gpio
)
