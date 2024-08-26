subdir-ccflags-y += -I$(srctree)/arch/arm64/boot/dts/mediatek/hq_chg
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/nu_charger
subdir-ccflags-y += -DCONFIG_ENABLE_BOOT_DEBUG -DCONFIG_ENABLE_SYSFS_DEBUG

obj-$(CONFIG_NU_CHARGERPUMP_NU2115)    += nu2115.o
obj-m   += nu2115.o
