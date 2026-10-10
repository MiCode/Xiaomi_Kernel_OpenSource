# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_LX_CHARGER_MANAGER)    += charger_policy/
obj-$(CONFIG_LX_CHARGER_CLASS)      += charger_class/
obj-$(CONFIG_LX_CHARGER_IC)         += charger_ic/
obj-y                               += fuelgauge/

ifeq ($(KERNEL_FACTORY_BUILD),1)
KBUILD_CFLAGS += -DKERNEL_FACTORY_BUILD=1
endif