set(include_dirs . web)

set(srcs 
    "main.c"
    "mcp3004.c"
    "SH1107.c"
    "fonts.c"
    "spi_common.c"
    "web/web_common.c"
    "web/ws.c"
    "web/wifi.c")

idf_component_register(
    SRCS "${srcs}" 
    INCLUDE_DIRS "${include_dirs}"
    EMBED_FILES web/page/dist/index.html)