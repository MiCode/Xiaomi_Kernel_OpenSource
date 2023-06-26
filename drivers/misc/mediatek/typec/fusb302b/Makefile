#
# Makefile for the usb FUSB302 driver (if driver is inside kernel tree).
#

ccflags-y += \
		-DFSC_PLATFORM_LINUX \
		-DFSC_HAVE_SNK \
		-DFSC_HAVE_SRC \
		-DFSC_HAVE_DRP \
		-DFSC_HAVE_ACCMODE \
		-DFSC_HAVE_EXT_MSG \
		-DFSC_DEBUG \
		-DFSC_HAVE_VDM \
		-DFSC_HAVE_DP

EXTRA_CFLAGS += -I$(src)/./core
EXTRA_CFLAGS += -I$(src)/./core/modules
EXTRA_CFLAGS += -I$(src)/./core/vdm
EXTRA_CFLAGS += -I$(src)/./core/vdm/DisplayPort

obj-y += fusb302_whole.o

fusb302_whole-objs := \
		./Platform_Linux/fusb30x_driver.o \
		./Platform_Linux/fusb30x_global.o \
		./Platform_Linux/platform.o \
		./Platform_Linux/platform_helpers.o \
		./Platform_Linux/dfs.o \
		./core/core.o \
		./core/PDPolicy.o \
		./core/timer.o \
		./core/fusb30X.o \
		./core/PDProtocol.o \
		./core/TypeC.o \
		./core/Log.o \
		./core/Port.o \
		./core/vendor_info.o \
		./core/modules/dpm.o \
		./core/modules/HostComm.o \
		./core/modules/observer.o \
		./core/vdm/bitfield_translators.o \
		./core/vdm/vdm.o \
		./core/vdm/vdm_callbacks.o \
		./core/vdm/DisplayPort/dp.o \
