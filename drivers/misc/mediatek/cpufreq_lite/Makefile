# SPDX-License-Identifier: GPL-2.0

ccflags-y += \
	-I$(srctree)/include/ \

obj-$(CONFIG_MEDIATEK_CPUFREQ_DEBUG_LITE) += cpudvfs.o
ifeq ($(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT), m)
subdir-ccflags-y += -DEEM_DBG_LITE
cpudvfs-y += eem-dbg-lite.o
endif
cpudvfs-y += cpufreq-dbg-lite.o
cpudvfs-y += devinfo.o
