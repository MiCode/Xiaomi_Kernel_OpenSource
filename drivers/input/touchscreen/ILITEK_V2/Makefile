BUILD_INFAE := spi
BUILD_PLATFORM := mtk
BUILD_MODULE := n

ccflags-y += -Wall

ifeq ($(BUILD_PLATFORM),mtk)
platform=ilitek_plat_mtk
ccflags-y += -I$(srctree)/drivers/spi/mediatek/mt6797/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/ILITEK_V2/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/ILITEK_V2/firmware/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
endif

ifeq ($(BUILD_PLATFORM),qcom)
platform=ilitek_plat_qcom
ccflags-y += -I$(srctree)/drivers/input/touchscreen/ILITEK_V2/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/ILITEK_V2/firmware/
endif

ifeq ($(BUILD_INFAE),i2c)
interface=ilitek_i2c
fwupdate=ilitek_flash
endif

ifeq ($(BUILD_INFAE),spi)
interface=ilitek_spi
fwupdate=ilitek_hostdl
endif

ifeq ($(BUILD_MODULE),n)
obj-y += ilitek_main.o \
	$(interface).o \
	$(platform).o \
	ilitek_ic.o \
	ilitek_touch.o \
	$(fwupdate).o \
	ilitek_node.o \
	tpd_control.o \
	tpd_button.o \
	tpd_debug.o \
	tpd_default.o \
	tpd_misc.o \
	tpd_setting.o
else
	obj-m += ilitek.o
	ilitek-y := ilitek_main.o \
		$(interface).o \
		$(platform).o \
		ilitek_ic.o \
		ilitek_touch.o \
		ilitek_mp.o \
		$(fwupdate).o \
		ilitek_node.o

endif
