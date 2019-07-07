subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include


subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6357)+= -I$(srctree)/drivers/misc/mediatek/accdet/mt6357
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6358)+= -I$(srctree)/drivers/misc/mediatek/accdet/mt6358
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6359)+= -I$(srctree)/drivers/misc/mediatek/accdet/mt6359

obj-$(CONFIG_MTK_ACCDET) := accdet_drv.o


obj-$(CONFIG_MTK_PMIC_CHIP_MT6357) += mt6357/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6358) += mt6358/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6359) += mt6359/
