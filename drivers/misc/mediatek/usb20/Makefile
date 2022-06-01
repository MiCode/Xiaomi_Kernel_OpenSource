#
# Copyright (C) 2017 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

#
# for USB OTG silicon based on Mentor Graphics INVENTRA designs
#

ccflags-y := -I$(srctree)/drivers/usb/gadget/
ccflags-y += -I$(srctree)/drivers/usb/gadget/function

ifeq ($(MTK_ALPS_BOX_SUPPORT),yes)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

# for SPM control usage
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include

# for musb main
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb20/musb_main/

# for typec event
ifeq ($(CONFIG_TCPC_CLASS),$(filter $(CONFIG_TCPC_CLASS),m y))
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
endif

# define_trace.h needs to know how to find our header
CFLAGS_musb_trace.o	:= -I$(src)

# Driver module ko musb_hdrc

obj-$(CONFIG_USB_MTK_HDRC)	+= musb_hdrc.o
musb_hdrc-y := musb_core.o musb_virthub.o musb_host.o musb_gadget_ep0.o musb_gadget.o musb_trace.o

ifneq ($(CONFIG_MTK_MUSB_DUAL_ROLE),)
	musb_hdrc-y	+= musb_dr.o
endif

ifneq ($(CONFIG_PROC_FS),)
	musb_hdrc-y	+= musb_debugfs.o
endif

ifneq ($(CONFIG_MTK_MUSB_QMU_SUPPORT),)
	musb_hdrc-y	+=  mtk_qmu.o musb_qmu.o
endif

musb_hdrc-y += musbhsdma.o usb20_host.o

obj-$(CONFIG_USB_MTK_HDRC)	+= musb_main/

