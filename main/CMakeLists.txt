idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        freertos
        log
        esp_system
        secure_storage
        wifi_manager
        mqtt_client
)
