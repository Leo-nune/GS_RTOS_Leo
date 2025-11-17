idf_component_register(SRCS "hello_world_main.c"
                    PRIV_REQUIRES spi_flash
                    PRIV_REQUIRES esp_wifi
                    PRIV_REQUIRES esp_event
                    PRIV_REQUIRES nvs_flash
                    INCLUDE_DIRS "")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
