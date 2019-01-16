include $(srctree)/drivers/misc/mediatek/Makefile.custom

GED_TOP = $(srctree)/drivers/misc/mediatek/gpu/ged

VER:=eng

ifneq ($(strip $(TARGET_BUILD_VARIANT)),)
ifneq ($(strip $(TARGET_BUILD_VARIANT)),eng)
VER:=user
endif
endif

ccflags-y += \
	-I$(GED_TOP)/include/

ccflags-y += \
	-include $(GED_TOP)/include/config_kernel_$(VER).h

obj-y += ged.o

ged-y += \
	src/ged_base.o \
	src/ged_main.o \
	src/ged_debugFS.o \
	src/ged_log.o \
	src/ged_hal.o \
	src/ged_bridge.o \
	src/ged_profile_dvfs.o \
	src/ged_monitor_3D_fence.o \
	src/ged_hashtable.o
