idf_component_register(
    SRCS
        "main.cpp"
        "../../shared/libs/oled_display/oled_display_wroom.cpp"
        "../../shared/libs/shared_bus/shared_bus_wroom.cpp"
    INCLUDE_DIRS
        "."
        "../../shared/config"
        "../../shared/libs/oled_display"
        "../../shared/libs/shared_bus"
    REQUIRES
        esp_common log freertos driver esp_driver_i2c esp_driver_gpio u8g2
)