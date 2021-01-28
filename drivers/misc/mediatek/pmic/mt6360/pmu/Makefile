subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/

obj-y							+= mt6360_pmu_dts.o
obj-$(CONFIG_MFD_MT6360_PMU)		+= mt6360_pmu_i2c.o mt6360_pmu_regmap.o\
					mt6360_pmu_irq.o mt6360_pmu_subdev.o\
					mt6360_pmu_core.o mt6360_pmu_adc.o
obj-$(CONFIG_MT6360_PMU_FLED)		+= mt6360_pmu_fled.o
obj-$(CONFIG_MT6360_PMU_RGBLED)		+= mt6360_pmu_rgbled.o mt_led_trigger.o

obj-$(CONFIG_MT6360_PMU_CHARGER)	+= mt6360_pmu_chg.o mt6360_pmu_adc.o
