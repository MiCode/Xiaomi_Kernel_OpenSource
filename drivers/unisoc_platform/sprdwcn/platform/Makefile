# SPDX-License-Identifier: GPL-2.0
#
# Makefile for Unisoc wcn platform configuration
#

#common obj
#GNSS
#WCN_MARLIN_COMMON_FILES += gnss/gnss_common_ctl.o \
#			   gnss/gnss_pmnotify_ctl.o \
#			   gnss/gnss_dbg.o
#			   gnss/gnss_dump.o

#WCN buf log loopcheck ...
WCN_MARLIN_COMMON_FILES += bufring.o \
			   wcn_txrx.o \
			   wcn_misc.o \
			   wcn_parn_parser.o \
			   wcn_log.o \
			   wcn_procfs.o \
			   loopcheck.o \
			   reset.o \
			   reset_test.o \
			   rdc_debug.o
#			   sysfs.o

#common obj end

ifeq ($(TARGET_BUILD_VARIANT),user)
ccflags-y += -DFLAG_WCN_USER
endif

obj-y := marlin.o
marlin-y += pcie_boot.o
marlin-y += wcn_swd_dap.o

#marlin3:sdio pcie
MARLINOFILES += wcn_boot.o \
		wcn_dump.o \
		wcn_gnss_dump.o \
		gnss_dump.o \
		wcn_op.o

marlin-y += mem_pd_mgr.o
marlin-$(CONFIG_DEBUG_FS) += wcn_debugfs.o

marlin-objs += $(WCN_MARLIN_COMMON_FILES)
marlin-objs += $(MARLINOFILES)

#WCN_PLATFORM
obj-y := wcn_platform.o

wcn_platform-c-objs += $(WCN_MARLIN_COMMON_FILES)

wcn_platform-c-objs += rf/rf.o
wcn_platform-c-objs +=  wcn_dump_integrate.o

wcn_platform-objs += $(wcn_platform-c-objs)
