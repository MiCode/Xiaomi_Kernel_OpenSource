$(warning shell echo "sprd_gnss entry")
#make single or multiple .ko for gnss modules
BUILD_SINGLE_GNSS_MODULE := false

$(warning shell echo "BUILD_SINGLE_GNSS_MODULE=${BUILD_SINGLE_GNSS_MODULE}")
$(warning shell echo "make a single ko for gnss:$(BUILD_SINGLE_GNSS_MODULE)")

ifeq ($(BUILD_SINGLE_GNSS_MODULE),true)
subdir-ccflags-y += -DGNSS_SINGLE_MODULE

obj-$(CONFIG_UNISOC_GNSS) := gnss_common_ctl_all.o

GNSSFILES += gnss_common_ctl/gnss_common_ctl.o
GNSSFILES += gnss_pmnotify_ctl/gnss_pmnotify_ctl.o
GNSSFILES += gnss_dbg/gnss_dbg.o

gnss_common_ctl_all-objs += $(GNSSFILES)
else
obj-$(CONFIG_UNISOC_GNSS)    += gnss_common_ctl/
obj-$(CONFIG_UNISOC_GNSS)    += gnss_pmnotify_ctl/
obj-$(CONFIG_UNISOC_GNSS)    += gnss_dbg/
endif
$(warning shell echo "sprd_gnss exit")
