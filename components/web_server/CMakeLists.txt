idf_component_register(
    SRCS "web_server.c" "web_pages.c" "web_api.c" "console_ws.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server wifi_manager error_manager motor esp_wifi esp_timer esp_ringbuf log
    PRIV_REQUIRES json
)
