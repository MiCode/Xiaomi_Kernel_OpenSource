#
# Makefile for misc devices that really don't fit anywhere else.
#

include $(srctree)/drivers/misc/mediatek/Makefile.custom
ifeq ($(CONFIG_CM865_MAINBOARD), y)
obj-y				+= leds_strobe_cm865.o
else
obj-y				+= leds_strobe.o
endif
