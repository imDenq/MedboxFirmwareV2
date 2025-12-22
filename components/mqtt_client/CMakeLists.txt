idf_component_register(
    SRCS "mqtt_client_medbox.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt esp-tls secure_storage esp_event
    EMBED_TXTFILES "${PROJECT_DIR}/certs/ca.crt"
                   "${PROJECT_DIR}/certs/client.crt"
                   "${PROJECT_DIR}/certs/client.key"
)
