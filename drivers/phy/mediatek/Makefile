#
# Makefile for the phy drivers.
#

obj-$(CONFIG_PHY_MTK_TPHY)		+= phy-mtk-tphy.o
obj-$(CONFIG_PHY_MTK_FPGAPHY)		+= phy-mtk-fpga.o

ifeq ($(CONFIG_PHY_MTK_USB),y)
obj-y					+= phy-mtk.o
obj-$(CONFIG_MACH_MT6885)		+= phy-mtk-mt6885.o
obj-$(CONFIG_MACH_MT6893)		+= phy-mtk-mt6885.o
obj-$(CONFIG_MACH_MT6877)		+= phy-mtk-mt6877.o
obj-$(CONFIG_MACH_MT6873)		+= phy-mtk-mt6873.o
obj-$(CONFIG_MACH_MT6853)		+= phy-mtk-mt6853.o
obj-$(CONFIG_MACH_MT6785)		+= phy-mtk-mt6785.o
obj-$(CONFIG_MACH_MT6771)		+= phy-mtk-mt6771.o
endif
