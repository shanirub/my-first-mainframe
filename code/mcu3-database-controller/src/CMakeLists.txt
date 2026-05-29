idf_component_register(
    SRCS
        "main.cpp"
        "../../shared/libs/oled_display/oled_display_wroom.cpp"
    INCLUDE_DIRS
        "."
        "../../shared/config"
        "../../shared/libs/oled_display"
    REQUIRES
        esp_common log freertos driver u8g2-hal-esp-idf
)