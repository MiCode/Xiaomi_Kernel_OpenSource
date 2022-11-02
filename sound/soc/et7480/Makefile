#
# Makefile for the kernel et7480 type-c switch device drivers.
#
obj-$(CONFIG_USB_SWITCH_ET7480) += et7480.o

MTK_CUSTOM_PATH := $(srctree)/drivers/misc/mediatek
subdir-ccflags-y += \
	-I$(MTK_CUSTOM_PATH)/typec
subdir-ccflags-y += \
	-I$(MTK_CUSTOM_PATH)/power_throttling
subdir-ccflags-y += \
    -I$(MTK_CUSTOM_PATH)/pbm

subdir-ccflags-y += -I$(srctree)/drivers/power/supply/
subdir-ccflags-y += -I$(srctree)/sound/soc/codecs/
