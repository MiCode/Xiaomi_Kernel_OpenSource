# Makefile for Intel display driver based on ADF framework

ccflags-y := -Idrivers/staging/android -Iinclude/adf -Iinclude/uapi/adf -Idrivers/video/adf/intel/include -Idrivers/video/adf/intel/core/common/dsi -Werror

CFLAGS_adf.o := -I$(src)

ifdef CONFIG_ADF_INTEL_VLV
	obj-y := intel_dsi_panel_vbt.o
else
	obj-y := cmi_7x12_cmd.o
	obj-y += jdi_7x12_cmd.o
      	obj-y += jdi_7x12_vid.o
	obj-y += sharp_10x19_cmd.o
	obj-y += sharp_25x16_cmd.o
	obj-y += sharp_25x16_vid.o
endif
