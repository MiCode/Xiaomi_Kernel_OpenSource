# Makefile for Intel display driver based on ADF framework

ccflags-y := -Idrivers/staging/ -Iinclude/adf -Iinclude/uapi/adf -Idrivers/video/adf/intel/include -Werror

CFLAGS_adf.o := -I$(src)

ifdef CONFIG_ADF_INTEL_VLV
	obj-y := gen_dsi_pipe.o
	obj-y += dsi_pipe_ops.o
else
	obj-y := dsi_pipe.o
	obj-y += dsi_dbi.o
	obj-y += dsi_dbi_dsr.o
	obj-y += dsi_dpi_sdo.o
	obj-y += dsi_pkg_sender.o
endif

obj-y += dsi_config.o
obj-y += dsi_panel.o

obj-y += panels/

