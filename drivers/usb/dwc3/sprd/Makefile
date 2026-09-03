# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_USB_DWC3_SPRD_CORE)	+= dwc3-sprd-core.o
dwc3-sprd-core-y			:= core.o

ifneq ($(CONFIG_TRACING),)
	dwc3-sprd-core-y		+= trace.o
endif

ifneq ($(filter y,$(CONFIG_USB_DWC3_HOST) $(CONFIG_USB_DWC3_DUAL_ROLE)),)
	dwc3-sprd-core-y		+= host.o
endif

ifneq ($(filter y,$(CONFIG_USB_DWC3_GADGET) $(CONFIG_USB_DWC3_DUAL_ROLE)),)
	dwc3-sprd-core-y		+= gadget.o ep0.o
endif

ifneq ($(CONFIG_USB_DWC3_DUAL_ROLE),)
	dwc3-sprd-core-y		+= drd.o
endif

ifneq ($(CONFIG_USB_DWC3_ULPI),)
	dwc3-sprd-core-y		+= ulpi.o
endif

ifneq ($(CONFIG_DEBUG_FS),)
	dwc3-sprd-core-y		+= debugfs.o
endif

## Add prefix to ignore conflict with build-in dwc3
OBJCOPYFLAGS_dwc3-sprd-core.o := -prefix-symbols=__sprd_
