#
# Makefile for the phy drivers.
#

obj-$(CONFIG_PHY_MTK_TPHY)		+= phy-mtk-tphy.o

ifeq ($(CONFIG_PHY_MTK_USB),y)
obj-y					+= phy-mtk.o
obj-$(CONFIG_MACH_MT6785)		+= phy-mtk-mt6785.o
obj-$(CONFIG_MACH_MT6771)		+= phy-mtk-mt6771.o
endif
