# SPDX-License-Identifier: GPL-2.0

subdir-ccflags-y += -Wformat

DSP_VERSION := hifi3_v2

# target object
obj-$(CONFIG_MTK_AUDIODSP_SUPPORT) += adsp.o

# common include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/common
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include

# platform include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(DSP_VERSION)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include

adsp-objs += common/adsp_core.o
adsp-objs += common/adsp_fs.o
adsp-objs += common/adsp_semaphore.o
adsp-objs += common/adsp_logger.o
adsp-objs += common/adsp_feature_table.o

adsp-objs += $(DSP_VERSION)/adsp_awake.o
adsp-objs += $(DSP_VERSION)/adsp_clk.o
adsp-objs += $(DSP_VERSION)/adsp_reserved_mem.o
adsp-objs += $(DSP_VERSION)/adsp_platform_driver.o
adsp-objs += $(DSP_VERSION)/adsp_platform.o
adsp-objs += $(DSP_VERSION)/adsp_mbox.o
adsp-objs += $(DSP_VERSION)/adsp_excep.o
#adsp-objs +=  $(DSP_VERSION)/adsp_bus_monitor.o

