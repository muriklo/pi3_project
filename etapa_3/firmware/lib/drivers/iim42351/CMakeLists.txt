# SysCare - driver do acelerometro IIM-42351

zephyr_library_named(syscare_iim42351)

zephyr_library_sources(
    drv_iim42351.c
    drv_iim42351_bus.c
)

zephyr_include_directories(.)
