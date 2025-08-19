idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash
                       REQUIRES driver
                       REQUIRES esp_timer
                       INCLUDE_DIRS "")
