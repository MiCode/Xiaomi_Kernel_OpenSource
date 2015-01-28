# Makefile for Intel display driver based on ADF framework
ccflags-y := -Idrivers/staging/ -Iinclude/adf -Iinclude/uapi/adf -Idrivers/video/adf/intel/include -Werror

obj-$(CONFIG_ADF_INTEL) += backlight_dev.o
obj-$(CONFIG_ADF_INTEL) += intel_gen_backlight.o
obj-y 			+= intel_adf_events.o

obj-y			+= dsi/
obj-y			+= hdmi/
obj-y			+= dp/
