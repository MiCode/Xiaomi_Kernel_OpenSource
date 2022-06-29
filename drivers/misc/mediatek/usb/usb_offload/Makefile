# SPDX-License-Identifier: GPL-2.0
#
# USB peripheral controller drivers
#

### for adsp
ifneq ($(CONFIG_MTK_AUDIODSP_SUPPORT), )
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
endif

ifneq ($(CONFIG_USB_XHCI_MTK), )
ccflags-y += -I$(srctree)/drivers/usb/host
endif

ifneq ($(CONFIG_SND_USB_AUDIO), )
ccflags-y += -I$(srctree)/sound/usb
endif

obj-$(CONFIG_MTK_USB_OFFLOAD)	+= usb_offload.o
