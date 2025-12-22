idf_component_register(
    SRCS "secure_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_partition
)
