# SysCare - servicos

zephyr_library_named(syscare_services)

zephyr_library_sources_ifdef(CONFIG_SYSCARE_SRV_ACCELEROMETER srv_accelerometer.c)

zephyr_include_directories(.)
