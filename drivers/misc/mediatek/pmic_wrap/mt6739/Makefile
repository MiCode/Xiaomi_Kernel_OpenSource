
ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
endif

ifeq ($(CONFIG_MTK_PMIC_CHIP_MT6357), y)
obj-$(CONFIG_MTK_PMIC_WRAP_HAL) +=  pwrap_hal_v1.o
else
obj-$(CONFIG_MTK_PMIC_WRAP_HAL) +=  pwrap_hal.o
endif
