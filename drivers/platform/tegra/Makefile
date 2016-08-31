#
# Makefile for linux/drivers/platform/tegra
# Tegra Platform-Specific Drivers
#
plus_sec := $(call as-instr,.arch_extension sec,+sec)
CFLAGS_tegra_sm_interface.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)

obj-$(CONFIG_USB_SUPPORT)               += tegra_usb_pmc.o
obj-$(CONFIG_TEGRA_USB_SHARED_PAD)      += tegra_usb_pad_ctrl.o
obj-$(CONFIG_TEGRA_FIQ_DEBUGGER)        += tegra_fiq_debugger.o
obj-$(CONFIG_TEGRA_USE_SECURE_KERNEL)   += tegra_sm_interface.o
