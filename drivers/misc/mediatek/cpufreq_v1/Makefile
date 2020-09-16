# SPDX-License-Identifier: GPL-2.0

ccflags-y += \
	-I$(srctree)/include/ \

obj-$(CONFIG_MEDIATEK_CPUFREQ_DEBUG) += cpuhvfs.o
ifeq ($(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT), m)
ifeq ($(CONFIG_MTK_IPI), m)
subdir-ccflags-y += -DEEM_DBG
cpuhvfs-objs += eem-dbg.o
endif
endif
cpuhvfs-objs += cpufreq-dbg.o

