# Makefile for Intel display driver based on ADF framework

ccflags-y := -Idrivers/staging/ -Iinclude/adf -Iinclude/uapi/adf -Idrivers/video/adf/intel/include -Werror

CFLAGS_adf.o := -I$(src)

ifdef CONFIG_ADF_INTEL_VLV
	obj-y := gen_hdmi_pipe.o
	obj-y += gen_hdmi_pipe_ops.o
	obj-y += gen_hdmi_edid.o
endif
