
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6336
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include

# Determine MT6336 is E1 or E2
_MT6336_E1 = y

# Mediatek PMIC MT63XX specific files
obj-$(CONFIG_MTK_PMIC_CHIP_MT6336)	+= mt6336.o mt6336_efuse.o mt6336_auxadc.o pmic_chr_type_det.o
ifdef _MT6336_E1
$(warning MT6336_E1)
	ccflags-y += -DMT6336_E1
	obj-$(CONFIG_MTK_PMIC_CHIP_MT6336)	+= mt6336_irq_e1.o mt6336_common_e1.o
else
	obj-$(CONFIG_MTK_PMIC_CHIP_MT6336)	+= mt6336_irq.o mt6336_common.o
endif
obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_mt6336.o
