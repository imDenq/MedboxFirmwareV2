idf_component_register(
    SRCS "main.c" "boot_ui.c"
    INCLUDE_DIRS "."
    REQUIRES
        freertos
        log
        esp_system
        secure_storage
        wifi_manager
        mqtt_client
        web_server
        json
)
