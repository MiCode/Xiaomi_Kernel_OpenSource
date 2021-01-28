# SPDX-License-Identifier: GPL-2.0
# Core
obj-y				+= pinctrl-mtk-common.o

ifneq ($(CONFIG_ARCH_MEDIATEK), n)
ccflags-y += -DCONFIG_PINCTRL_MTK_ALTERNATIVE
endif

# SoC Drivers
obj-$(CONFIG_PINCTRL_MT2701)	+= pinctrl-mt2701.o
obj-$(CONFIG_PINCTRL_MT8135)	+= pinctrl-mt8135.o
obj-$(CONFIG_PINCTRL_MT8127)	+= pinctrl-mt8127.o
obj-$(CONFIG_PINCTRL_MT8168)	+= pinctrl-mt8168.o
obj-$(CONFIG_PINCTRL_MT8173)	+= pinctrl-mt8173.o
obj-$(CONFIG_PINCTRL_MT8183)	+= pinctrl-mt8183.o
obj-$(CONFIG_PINCTRL_MT6397)	+= pinctrl-mt6397.o
obj-$(CONFIG_PINCTRL_MT6771)	+= pinctrl-mt6771.o
