idf_component_register(
    SRCS
        "main.c"
        "led_controller.c"
        "patterns.c"
        "wifi_ap.c"
        "dns_server.c"
        "web_server.c"
        "nvs_store.c"
    INCLUDE_DIRS "."
    EMBED_FILES "web/index.html"
)
