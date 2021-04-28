ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic_wrap/$(CONFIG_MTK_PLATFORM)

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
endif

obj-$(CONFIG_MTK_PMIC_COMMON) += upmu.o upmu_regulator.o upmu_debugfs.o upmu_initial_setting.o
ifeq ($(CONFIG_MTK_PMIC_CHIP_MT6359), y)
obj-y += upmu_lbat_service_v2.o
else ifeq ($(CONFIG_MTK_PMIC_CHIP_MT6359P), y)
obj-y += upmu_lbat_service_v2.o
else
obj-y += upmu_lbat_service.o
endif
