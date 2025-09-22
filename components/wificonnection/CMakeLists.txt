idf_component_register(SRCS "wificonnection.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_wifi esp_event nvs_flash json)
