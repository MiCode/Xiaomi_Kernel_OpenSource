subdir-ccflags-y += -I$(srctree)/arch/arm64/boot/dts/mediatek/hq_chg
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/nu_charger
subdir-ccflags-y += -DCONFIG_ENABLE_BOOT_DEBUG -DCONFIG_ENABLE_SYSFS_DEBUG

obj-$(CONFIG_NU_SUBPMIC_NU6601)    += subpmic_nu.o subpmic_base.o

obj-$(CONFIG_NU_SUBPMIC_NU6601)	+= subpmic_nu.o
subpmic_nu-objs := subpmic_nu6601.o nu6601_qc35.o
