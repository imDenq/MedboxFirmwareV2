idf_component_register(
    SRCS "motor.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_rom freertos log nvs_flash
)
