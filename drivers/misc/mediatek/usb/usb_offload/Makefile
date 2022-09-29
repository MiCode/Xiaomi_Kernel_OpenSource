# SPDX-License-Identifier: GPL-2.0
#
# USB peripheral controller drivers
#

### for adsp
ifneq ($(CONFIG_MTK_AUDIODSP_SUPPORT), )
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
endif

ccflags-y += -I$(srctree)/drivers/clk/mediatek/

ifneq ($(CONFIG_DEVICE_MODULES_USB_XHCI_MTK), )
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb/usb_xhci
else
ifneq ($(CONFIG_USB_XHCI_MTK), )
ccflags-y += -I$(srctree)/drivers/usb/host
endif
endif

ifneq ($(CONFIG_SND_USB_AUDIO), )
ccflags-y += -I$(srctree)/sound/usb
endif

obj-$(CONFIG_MTK_USB_OFFLOAD)	+= usb_offload.o
