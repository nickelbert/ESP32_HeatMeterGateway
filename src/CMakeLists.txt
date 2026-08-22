idf_component_register(
    SRCS "Main.cpp" "ConfigManager.cpp" "WifiManager.cpp" "TelnetServer.cpp" "WebServer.cpp" "MeterT550.cpp" "MqttHandler.cpp"
    INCLUDE_DIRS "." "../include"
    REQUIRES esp_timer esp_wifi esp_event esp_netif nvs_flash esp_http_server driver lwip mqtt json app_update esp_http_client esp_https_ota
)
