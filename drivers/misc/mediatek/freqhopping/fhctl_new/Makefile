obj-y += fhctl.o
fhctl-y += clk-fhctl.o
fhctl-y += clk-fhctl-ap.o
fhctl-y += clk-fhctl-pll.o
ifeq ($(CONFIG_MACH_MT6833),y)
ifeq ($(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcupm/$(MTK_PLATFORM)/
ccflags-y += -DUSE_FHCTL_MCUPM
fhctl-y += clk-fhctl-mcupm.o
endif
endif
fhctl-$(CONFIG_DEBUG_FS) += clk-fhctl-debug.o
