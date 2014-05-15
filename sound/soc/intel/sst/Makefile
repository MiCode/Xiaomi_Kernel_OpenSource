# Makefile for SST Audio driver
snd-intel-sst-objs := sst.o sst_ipc.o sst_stream.o sst_drv_interface.o sst_dsp.o sst_pvt.o sst_app_interface.o sst_acpi.o

ifdef CONFIG_DEBUG_FS
	snd-intel-sst-objs += sst_debug.o
endif

obj-$(CONFIG_SND_INTEL_SST) += snd-intel-sst.o


CFLAGS_snd-intel-sst.o = -I$(src)

ccflags-y += -DMRFLD_WORD_WA -Werror

