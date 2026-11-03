# SysCare - driver do acelerometro IIM-42351

zephyr_library_named(syscare_iim42351)

zephyr_library_sources(
    drv_iim42351.c
    drv_iim42351_bus.c
    drv_iim42351_setup.c
)

zephyr_library_sources_ifdef(CONFIG_SYSCARE_IIM42351_TRIGGER drv_iim42351_trigger.c)

zephyr_include_directories(.)
