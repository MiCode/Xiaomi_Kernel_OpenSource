ifeq ($(CONFIG_MACH_MT6768),y)
obj-y += v1/
else ifeq ($(CONFIG_MACH_MT6781),y)
obj-y += v1/
else
subdir-ccflags-y := -Wall -Werror $(call cc-disable-warning, unused-but-set-variable)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/misc
subdir-ccflags-y += -I$(srctree)/drivers/power/supply
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb20

obj-$(CONFIG_MFD_MT6370_PMU)		+= mt6370_pmu_i2c.o mt6370_pmu_regmap.o\
					mt6370_pmu_irq.o mt6370_pmu_subdevs.o\
					mt6370_pmu_core.o
obj-$(CONFIG_MT6370_PMU_LDO)		+= mt6370_pmu_ldo.o
obj-$(CONFIG_MT6370_PMU_DSV)		+= mt6370_pmu_dsv.o mt6370_pmu_dsv_debugfs.o
obj-$(CONFIG_MT6370_PMU_FLED)		+= mt6370_pmu_fled.o
obj-$(CONFIG_MT6370_PMU_BLED)		+= mt6370_pmu_bled.o
obj-$(CONFIG_MT6370_PMU_RGBLED)		+= mt6370_pmu_rgbled.o
obj-$(CONFIG_MT6370_PMU_CHARGER)	+= mt6370_pmu_charger.o
endif
