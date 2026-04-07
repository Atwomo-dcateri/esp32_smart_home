idf_component_register(
    SRCS esp_http_ota.c
    INCLUDE_DIRS include
    REQUIRES
        app_update      # 对应 esp_ota_ops.h
        esp_https_ota   # 对应 esp_https_ota.h
        mbedtls         # 对应 esp_crt_bundle.h   esp_crt_bundle 
        esp_http_client # esp_https_ota 依赖此组件进行网络传输
)
 