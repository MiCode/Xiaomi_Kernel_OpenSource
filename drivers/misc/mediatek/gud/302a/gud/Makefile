#
# Makefile for the kernel mobicore drivers
#

ifneq ($(MTK_ROOT_BUILD),)
include $(MTK_ROOT_BUILD)/Makefile
endif

GUD_ROOT_FOLDER := $(dir $(lastword $(MAKEFILE_LIST)))
# add our modules to kernel.
obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += mcKernelApi.o
obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += mcDrvModule.o
obj-$(CONFIG_TRUSTONIC_TRUSTED_UI) += TlcTui.o

mcDrvModule-objs := MobiCoreDriver/logging.o \
		MobiCoreDriver/ops.o \
		MobiCoreDriver/mem.o \
		MobiCoreDriver/api.o \
		MobiCoreDriver/pm.o \
		MobiCoreDriver/main.o

mcKernelApi-objs := MobiCoreKernelApi/main.o \
		MobiCoreKernelApi/clientlib.o \
		MobiCoreKernelApi/device.o \
		MobiCoreKernelApi/session.o \
		MobiCoreKernelApi/connection.o

TlcTui-objs := TlcTui/main.o \
		       TlcTui/tlcTui.o \
		       TlcTui/trustedui.o \
		       TlcTui/tui-hal_$(MTK_PLATFORM).o

# Release mode by default
ccflags-y := -DNDEBUG
ccflags-y += -Wno-declaration-after-statement
#ccflags-y += -Wno-error=date-time

ccflags-$(CONFIG_MOBICORE_DEBUG) += -DDEBUG
ccflags-$(CONFIG_MOBICORE_VERBOSE) += -DDEBUG_VERBOSE

# Choose one platform from the folder
#MOBICORE_PLATFORM := $(shell (ls -1 $(GUD_ROOT_FOLDER)MobiCoreDriver/platforms | tail -1) )
ccflags-y += -DMC_NETLINK_COMPAT_V37

# Use the available platform folder
#ccflags-y += -I$(GUD_ROOT_FOLDER)MobiCoreDriver/platforms/$(MOBICORE_PLATFORM)
# MobiCore Driver includes
ccflags-y += -I$(GUD_ROOT_FOLDER)MobiCoreDriver/public
# MobiCore KernelApi required incldes
ccflags-y += -I$(GUD_ROOT_FOLDER)MobiCoreKernelApi/include \
		-I$(GUD_ROOT_FOLDER)MobiCoreKernelApi/public

# MobiCore TlcTui required includes
ccflags-y += -I$(GUD_ROOT_FOLDER)/TlcTui \
		     -I$(GUD_ROOT_FOLDER)/TlcTui/inc \
		     -I$(GUD_ROOT_FOLDER)/TlcTui/public \
             include
