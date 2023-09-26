ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
ifeq ($(CONFIG_AGATE_CHARGER),y)
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger_agate
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc_agate/inc
else ifeq ($(CONFIG_ARES_CHARGER),y)
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger_ares
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc_ares/inc
else ifeq ($(CONFIG_PISSARRO_CHARGER),y)
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger_pissarro
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc_pissarro/inc
else
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
endif
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/misc
endif

obj-$(CONFIG_MFD_MT6360_PMU)		+= mt6360_pmu_i2c.o mt6360_pmu_regmap.o\
					mt6360_pmu_irq.o mt6360_pmu_subdev.o\
					mt6360_pmu_core.o mt6360_pmu_adc.o
obj-$(CONFIG_MT6360_PMU_FLED)		+= mt6360_pmu_fled.o
obj-$(CONFIG_MT6360_PMU_RGBLED)		+= mt6360_pmu_rgbled.o mt_led_trigger.o

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
ifeq ($(CONFIG_MT6360_PMU_CHARGER),y)
obj-y += mt6360_pmu_adc.o
ifeq ($(CONFIG_PISSARRO_CHARGER),y)
obj-y += mt6360_pmu_chg_pissarro.o
else
obj-y += mt6360_pmu_chg.o
endif
endif
endif
