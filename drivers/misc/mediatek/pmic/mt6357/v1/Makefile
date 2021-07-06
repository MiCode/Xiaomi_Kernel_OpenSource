ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic_wrap/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/power/mediatek/battery
ccflags-y += -I$(srctree)/drivers/power/mediatek/charger
ccflags-y += -I$(srctree)/drivers/power/mediatek/misc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb_c/tcpc/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/misc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc

subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/pmic/include

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
obj-y += pmic_ipi.o
endif

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
obj-y += mt6357_gauge.o
obj-y += mt6357_charger.o
endif

obj-$(CONFIG_MTK_PMIC_CHIP_MT6357)	+= pmic.o pmic_common.o \
		pmic_debugfs.o \
		pmic_irq.o \
		pmic_initial_setting.o \
		pmic_api.o \
		pmic_auxadc.o \
		pmic_throttling_dlpt.o \
		regulator_codegen.o \
		pmic_lp_api.o \
		pmic_efuse.o \

# Charger Type Detection
ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
	obj-$(CONFIG_MTK_PMIC_CHIP_MT6357) += pmic_chr_type_det_v2.o
else
	obj-$(CONFIG_MTK_PMIC_CHIP_MT6357) += pmic_chr_type_det.o
endif
