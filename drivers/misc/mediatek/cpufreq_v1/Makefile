# SPDX-License-Identifier: GPL-2.0

ccflags-y += \
	-I$(srctree)/include/ \

obj-$(CONFIG_MEDIATEK_CPUFREQ_DEBUG) += cpuhvfs.o
ifeq ($(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT), m)
ifeq ($(CONFIG_MTK_IPI), m)

ifeq ($(findstring _mcl,$(PROJECT_DTB_NAMES)),_mcl)
	subdir-ccflags-y += -DMC50_LOAD
endif

subdir-ccflags-y += -DEEM_DBG
cpuhvfs-y += eem-dbg.o
endif
endif
cpuhvfs-y += cpufreq-dbg.o

