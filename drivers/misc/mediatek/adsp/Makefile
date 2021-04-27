# SPDX-License-Identifier: GPL-2.0

subdir-ccflags-y += -Wformat

DSP_VERSION := hifi3_v2

# target object
obj-$(CONFIG_MTK_AUDIODSP_SUPPORT) += adsp_hifi3_v2.o

# common include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/common
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include

# platform include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(DSP_VERSION)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include

adsp_hifi3_v2-objs += common/adsp_core.o
adsp_hifi3_v2-objs += common/adsp_fs.o
adsp_hifi3_v2-objs += common/adsp_semaphore.o
adsp_hifi3_v2-objs += common/adsp_logger.o
adsp_hifi3_v2-objs += common/adsp_feature_table.o
adsp_hifi3_v2-objs += common/adsp_timesync.o

adsp_hifi3_v2-objs += $(DSP_VERSION)/adsp_awake.o
adsp_hifi3_v2-objs += $(DSP_VERSION)/adsp_clk.o
adsp_hifi3_v2-objs += $(DSP_VERSION)/adsp_reserved_mem.o
adsp_hifi3_v2-objs += $(DSP_VERSION)/adsp_platform_driver.o
adsp_hifi3_v2-objs += $(DSP_VERSION)/adsp_platform.o
adsp_hifi3_v2-objs += $(DSP_VERSION)/adsp_mbox.o
adsp_hifi3_v2-objs += $(DSP_VERSION)/adsp_excep.o
#adsp_hifi3_v2-objs +=  $(DSP_VERSION)/adsp_bus_monitor.o

