# SysCare - drivers de dispositivos
#
# Cada driver so entra no build se o Kconfig dele estiver habilitado
# (o que, por sua vez, depende de existir o no no devicetree).

add_subdirectory_ifdef(CONFIG_SYSCARE_IIM42351 iim42351)
