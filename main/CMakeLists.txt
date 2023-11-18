idf_component_register(SRCS main.c display.c connect.c stats.c
                       PRIV_REQUIRES
                            nvs_flash
                            esp_event esp_netif driver esp_wifi
                            download_file)
