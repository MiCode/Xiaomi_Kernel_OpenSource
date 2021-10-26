# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-$(CONFIG_POWER_SUPPLY_DEBUG) := -DDEBUG

power_supply-y				:= power_supply_core.o
power_supply-$(CONFIG_SYSFS)		+= power_supply_sysfs.o
power_supply-$(CONFIG_LEDS_TRIGGERS)	+= power_supply_leds.o
obj-$(CONFIG_POWER_SUPPLY)		+= power_supply.o

obj-y += pmic_voter.o

obj-y += mediatek/
obj-y += ti/
obj-y += mps/
obj-y += southchip/
obj-y += xiaomi/
obj-y += maxim/
