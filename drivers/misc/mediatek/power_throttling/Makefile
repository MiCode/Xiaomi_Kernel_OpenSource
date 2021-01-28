# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING) += mtk_battery_oc_throttling.o
obj-$(CONFIG_MTK_BATTERY_PERCENTAGE_POWER_THROTTLING) += mtk_battery_percentage_throttling.o
obj-$(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING) += mtk_low_battery_throttling.o
ifdef CONFIG_REGULATOR_MT6357
obj-$(CONFIG_PMIC_LBAT_SERVICE) += pmic_lbat_service_mt6357.o
else
obj-$(CONFIG_PMIC_LBAT_SERVICE) += pmic_lbat_service.o
endif
