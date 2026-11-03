# Removed vs original MiniNav:
#   - demo.cpp / demo.h264  (saved ~1.65 MB Flash)
#   - sdmmc / fatfs / esp_driver_sdmmc  (SD card dump feature gone)
#   - esp_driver_usb_serial_jtag  (USB console still works via IDF built-in; no explicit dep needed)
#
# Added / changed:
#   - board.cpp now uses LovyanGFX Touch_XPT2046 for the HR2046 resistive touch panel
#   - No IMU (QMI8658) code; no CST3530 capacitive touch code

idf_component_register(
    SRCS "app_main.cpp" "board.cpp" "net.cpp" "aap.cpp" "video.cpp"
    INCLUDE_DIRS "."
    REQUIRES LovyanGFX esp_wifi esp_netif esp_event nvs_flash mbedtls esp_timer
             esp_driver_gpio esp_driver_usb_serial_jtag lwip
)
