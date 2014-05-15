#EXTRA CFLAGS
ccflags-y += -Werror

#ifeq (${TARGET_BUILD_VARIANT},$(filter ${TARGET_BUILD_VARIANT}, eng))
ccflags-y += -DCONFIG_SND_VERBOSE_PRINTK -DCONFIG_SND_DEBUG -DCONFIG_SND_DEBUG_VERBOSE
#endif

# SST Platform Driver
PLATFORM_LIBS = platform-libs/controls_v2.o platform-libs/controls_v2_dpcm.o \
		platform-libs/ipc_lib_v2.o

snd-soc-sst-platform-objs := pcm.o compress.o effects.o $(PLATFORM_LIBS)
obj-$(CONFIG_SND_SST_PLATFORM) += snd-soc-sst-platform.o

# Relevant Machine driver
obj-$(CONFIG_SND_SST_MACHINE) += board/

# DSP driver
obj-$(CONFIG_SND_INTEL_SST) += sst/
