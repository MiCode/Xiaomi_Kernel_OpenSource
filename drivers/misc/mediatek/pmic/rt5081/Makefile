ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/misc
else
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/power/mt6757
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/misc
endif

obj-$(CONFIG_MFD_RT5081_PMU)		+= rt5081_pmu_i2c.o rt5081_pmu_regmap.o\
					rt5081_pmu_irq.o rt5081_pmu_subdevs.o\
					rt5081_pmu_core.o
obj-$(CONFIG_RT5081_PMU_LDO)		+= rt5081_pmu_ldo.o
obj-$(CONFIG_RT5081_PMU_DSV)		+= rt5081_pmu_dsv.o
obj-$(CONFIG_RT5081_PMU_FLED)		+= rt5081_pmu_fled.o
obj-$(CONFIG_RT5081_PMU_BLED)		+= rt5081_pmu_bled.o
obj-$(CONFIG_RT5081_PMU_RGBLED)		+= rt5081_pmu_rgbled.o

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
obj-$(CONFIG_RT5081_PMU_CHARGER)	+= rt5081_pmu_charger.o
else
obj-$(CONFIG_RT5081_PMU_CHARGER)	+= rt5081_pmu_charger_gm20.o
endif
