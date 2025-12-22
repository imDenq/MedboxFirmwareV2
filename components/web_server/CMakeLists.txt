idf_component_register(
    SRCS "web_server.c" "web_pages.c" "web_api.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server wifi_manager error_manager esp_wifi esp_timer
    PRIV_REQUIRES json
)
