BUILD_INFAE := spi
BUILD_PLATFORM := mtk
BUILD_MODULE := n

ccflags-y += -Wall

ifeq ($(BUILD_PLATFORM),mtk)
platform=ilitek_v3_mtk
ccflags-y += -I$(srctree)/drivers/spi/mediatek/mt6797/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/ILITEK_V3/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/ILITEK_V3/firmware/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
endif

ifeq ($(BUILD_INFAE),i2c)
interface=ilitek_v3_i2c
fwupdate=ilitek_v3_flash
endif

ifeq ($(BUILD_INFAE),spi)
interface=ilitek_v3_spi
fwupdate=ilitek_v3_hostdl
endif

ifeq ($(BUILD_MODULE),n)
obj-y += ilitek_v3.o \
	$(interface).o \
	$(platform).o \
	ilitek_v3_ic.o \
	ilitek_v3_touch.o \
	$(fwupdate).o \
	ilitek_v3_node.o \
	tpd_control.o \
	tpd_button.o \
	tpd_debug.o \
	tpd_default.o \
	tpd_misc.o \
	tpd_setting.o
else
	obj-m += ilitek.o
	ilitek-y := ilitek_v3.o \
		$(interface).o \
		$(platform).o \
		ilitek_v3_ic.o \
		ilitek_v3_touch.o \
		$(fwupdate).o \
		ilitek_v3_node.o
endif