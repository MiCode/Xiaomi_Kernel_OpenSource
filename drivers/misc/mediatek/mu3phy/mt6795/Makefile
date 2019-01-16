include $(srctree)/drivers/misc/mediatek/Makefile.custom

ifeq ($(CONFIG_USB_DEBUG),y)
	EXTRA_CFLAGS += -DDEBUG
endif

#For USB HQA Driving Tuning Mode 1 Settings //liuchao for usb yantu
EXTRA_CFLAGS += -DMTK_USB_MODE1

obj-$(CONFIG_MU3_PHY) := mu3phy.o

mu3phy-y := mtk-phy.o
mu3phy-$(CONFIG_U3_PHY_GPIO_SUPPORT) += mtk-phy-gpio.o 
mu3phy-$(CONFIG_U3_PHY_AHB_SUPPORT) += mtk-phy-ahb.o
mu3phy-$(CONFIG_C60802_SUPPORT) += mtk-phy-c60802.o
mu3phy-$(CONFIG_D60802_SUPPORT) += mtk-phy-d60802.o
mu3phy-$(CONFIG_E60802_SUPPORT) += mtk-phy-e60802.o
mu3phy-$(CONFIG_PROJECT_PHY) += mtk-phy-asic.o
