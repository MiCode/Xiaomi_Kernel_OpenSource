# SPDX-License-Identifier: GPL-2.0
# Core
obj-$(CONFIG_PINCTRL_MTK)	+= pinctrl-mtk-common.o
ifneq ($(CONFIG_ARCH_MEDIATEK), n)
ccflags-y += -DCONFIG_PINCTRL_MTK_ALTERNATIVE
endif
obj-$(CONFIG_EINT_MTK)		+= mtk-eint.o


obj-$(CONFIG_PINCTRL_MTK_MOORE) += pinctrl-moore.o pinctrl-mtk-common-v2.o
obj-$(CONFIG_PINCTRL_MTK_PARIS) += pinctrl-paris.o pinctrl-mtk-common-v2.o pinctrl-mtk-common-v2_debug.o

# SoC Drivers
obj-$(CONFIG_PINCTRL_MT2701)	+= pinctrl-mt2701.o
obj-$(CONFIG_PINCTRL_MT8135)	+= pinctrl-mt8135.o
obj-$(CONFIG_PINCTRL_MT8127)	+= pinctrl-mt8127.o
obj-$(CONFIG_PINCTRL_MT8167)	+= pinctrl-mt8167.o
obj-$(CONFIG_PINCTRL_MT8168)	+= pinctrl-mt8168.o
obj-$(CONFIG_PINCTRL_MT8173)	+= pinctrl-mt8173.o
obj-$(CONFIG_PINCTRL_MT8183)	+= pinctrl-mt8183.o
obj-$(CONFIG_PINCTRL_MT6397)	+= pinctrl-mt6397.o
obj-$(CONFIG_PINCTRL_MT6739)	+= pinctrl-mt6739.o
obj-$(CONFIG_PINCTRL_MT6765)	+= pinctrl-mt6765.o
obj-$(CONFIG_PINCTRL_MT6768)	+= pinctrl-mt6768.o
obj-$(CONFIG_PINCTRL_MT6771)	+= pinctrl-mt6771.o
obj-$(CONFIG_PINCTRL_MT6785)	+= pinctrl-mt6785.o
obj-$(CONFIG_PINCTRL_MT6833)	+= pinctrl-mt6833.o
obj-$(CONFIG_PINCTRL_MT6853)	+= pinctrl-mt6853.o
obj-$(CONFIG_PINCTRL_MT6873)	+= pinctrl-mt6873.o
obj-$(CONFIG_PINCTRL_MT6885)	+= pinctrl-mt6885.o
