# SysCare - HAL

zephyr_library_named(syscare_hal)

zephyr_library_sources_ifdef(CONFIG_SYSCARE_HAL_SPI hal_spi.c)
zephyr_library_sources_ifdef(CONFIG_SYSCARE_HAL_GPIO hal_gpio.c)

zephyr_include_directories(.)
