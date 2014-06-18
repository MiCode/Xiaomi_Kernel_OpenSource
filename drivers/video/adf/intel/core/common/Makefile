# Makefile for Intel display driver based on ADF framework

ccflags-y := -Idrivers/staging/ -Iinclude/adf -Iinclude/uapi/adf -Idrivers/video/adf/intel/include

CFLAGS_adf.o := -I$(src)

obj-$(CONFIG_ADF_INTEL) += backlight_dev.o
