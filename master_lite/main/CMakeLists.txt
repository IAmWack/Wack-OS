idf_component_register(
    SRCS "main.c"
         "display.c"
         "boot_button.c" # временно вместо "encoder.c" — см. board_pins.h, PIN_BOOT_BTN
         "sdcard.c"
         "clock.c"
         "sound.c"
         "battery.c"
         "power.c"
         "mpu6050.c"
         "theme.c"
         "settings.c"
         "astolfo.c"
         "razor.c"
         "venti.c"
         "portrait_sd.c"
         "portrait_fallback_data.c"
         "mascot.c"
         "ble_hid_device.c"
         "ui.c"
         "ui_widgets.c"
         "screens/screen_boot.c"
         "screens/screen_home.c"
         "screens/screen_menu.c"
         "screens/screen_settings.c"
         "screens/screen_about.c"
         "screens/screen_files.c"
         "screens/screen_clock.c"
         "screens/screen_remote.c"
         "screens/screen_remote_keyboard.c"
         "screens/screen_remote_mouse.c"
         "screens/screen_ble_scan.c"
    INCLUDE_DIRS "." "screens"
    REQUIRES wackos_gfx wackos_encoder
             esp_driver_gpio esp_driver_spi esp_driver_ledc esp_driver_i2c
             esp_driver_sdmmc sdmmc fatfs vfs
             esp_lcd esp_timer esp_adc
             nvs_flash bt esp_hid esp_event esp_hw_support
)
