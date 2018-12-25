subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6335)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6335
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6353)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6353
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6355)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6355
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6356)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6356
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6357)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6357

obj-$(CONFIG_MTK_EXTBUCK) += extbuck/
obj-$(CONFIG_MTK_PMIC_COMMON) += common/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6313) += mt6313/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6335) += mt6335/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6336) += mt6336/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6337) += mt6337/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6353) += mt6353/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6355) += mt6355/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6356) += mt6356/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6357) += mt6357/

obj-$(CONFIG_MTK_AUXADC_INTF) += common/upmu_auxadc.o

obj-$(CONFIG_MFD_RT5081_PMU)	+= rt5081/
obj-$(CONFIG_MFD_MT6370_PMU)    += mt6370/

ifneq (,$(findstring evb,$(CONFIG_ARCH_MTK_PROJECT)))
	subdir-ccflags-y += -DMTK_EVB_PLATFORM
endif

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/misc
obj-y += mtk_battery_adc_intf.o
endif
