ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic_wrap/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/misc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb_c/tcpc/inc

subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/pmic/include

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ifeq ($(CONFIG_MACH_MT6781),y)
# SSPM V2
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/mt6781/
obj-y += pmic_ipi_v2.o
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ifneq ($(CONFIG_MTK_TINYSYS_SSPM_VERSION),"")
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_TINYSYS_SSPM_VERSION)/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
endif
obj-y += pmic_ipi.o
endif
endif

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
ifeq ($(CONFIG_MACH_MT6781),y)
obj-y += mt6366_gauge.o
else
obj-y += mt6358_gauge.o
endif
endif

obj-$(CONFIG_MTK_PMIC_CHIP_MT6358)	+= pmic.o pmic_common.o \
		pmic_debugfs.o \
		pmic_irq.o \
		pmic_initial_setting.o \
		pmic_api.o \
		pmic_auxadc.o \
		regulator_codegen.o \
		pmic_lp_api.o \
		pmic_efuse.o \
		pmic_throttling_dlpt.o \

		#TODO
		#pmic_bif.o \

ifndef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
obj-$(CONFIG_MTK_PMIC_CHIP_MT6358) += pmic_dummy_chr_type_det.o
endif

