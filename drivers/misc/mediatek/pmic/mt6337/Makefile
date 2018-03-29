ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6337
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic_wrap/$(CONFIG_MTK_PLATFORM)

# Mediatek PMIC MT63XX specific files
obj-$(CONFIG_MTK_PMIC_CHIP_MT6337) += mt6337.o mt6337_irq.o mt6337_common.o
obj-$(CONFIG_MTK_PMIC_CHIP_MT6337) += mt6337_api.o mt6337_auxadc.o
