# SysCare - CMake do modulo compartilhado
#
# Cada componente (hal, drivers, services) entra aqui conforme for criado.
# Dentro de cada um, add_subdirectory_ifdef(CONFIG_..., <pasta>) garante
# que so o que esta habilitado no Kconfig entra no build.

add_subdirectory_ifdef(CONFIG_SYSCARE_HAL hal)
add_subdirectory(drivers)
