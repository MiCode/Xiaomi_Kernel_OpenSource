HAS_BOOTINFO_SRC_DIR ?= false
ifneq ($(HAS_BOOTINFO_SRC_DIR),true)
include $(srctree)/techpack/bootinfo/bootinfo.conf
LINUXINCLUDE    += \
		-include $(srctree)/techpack/bootinfo/bootinfoconf.h
ccflags-y := -Wno-non-literal-null-conversion
obj-$(CONFIG_BOOT_INFO)		+= bootinfo.o
endif # HAS_SCHED_SRC_DIR
