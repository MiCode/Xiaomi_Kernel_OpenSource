# Makefile for Intel display driver based on ADF framework
ccflags-y := -Idrivers/staging/ -Iinclude/adf -Iinclude/uapi/adf -Idrivers/video/adf/intel/include -Werror

obj-$(CONFIG_ADF_INTEL) += intel_adf.o \
	intel_adf_device.o \
	intel_adf_interface.o \
	intel_adf_overlay_engine.o \
	intel_adf_sync.o

obj-$(CONFIG_ADF_INTEL_FBDEV) += intel_adf_fbdev.o

ifndef CONFIG_ADF_INTEL_VLV
obj-$(CONFIG_ADF_MEMBLOCK) += intel_adf_mm.o
endif

obj-$(CONFIG_ADF_INTEL_FBDEV) += intel_adf_fbdev.o

obj-$(CONFIG_ADF_INTEL) += core/
