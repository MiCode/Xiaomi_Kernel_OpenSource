#
# Makefile for the mediatek phy drivers.
#
obj-y	+= phy-mtk.o
ifneq ($(CONFIG_PHY_MTK_SSUSB),)
	obj-y	+= phy-mtk-ssusb.o
else
	obj-$(CONFIG_MACH_MT6758) += phy-mtk-mt6758.o
endif