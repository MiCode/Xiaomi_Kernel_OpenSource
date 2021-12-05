# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

ifneq ($(wildcard $(srctree)/$(src)/devapc-mt6853.c),)
obj-$(CONFIG_DEVAPC_MT6853) += device-apc-mt6853.o
device-apc-mt6853-$(CONFIG_DEVAPC_MT6853) += devapc-mt6853.o
device-apc-mt6853-$(CONFIG_DEVAPC_ARCH_MULTI) += devapc-mtk-multi-ao.o
endif

ifneq ($(wildcard $(srctree)/$(src)/devapc-mt6855.c),)
obj-$(CONFIG_DEVAPC_MT6855) += device-apc-mt6855.o
device-apc-mt6855-$(CONFIG_DEVAPC_MT6855) += devapc-mt6855.o
device-apc-mt6855-$(CONFIG_DEVAPC_ARCH_MULTI) += devapc-mtk-multi-ao.o
endif

ifneq ($(wildcard $(srctree)/$(src)/devapc-mt6873.c),)
obj-$(CONFIG_DEVAPC_MT6873) += device-apc-mt6873.o
device-apc-mt6873-$(CONFIG_DEVAPC_MT6873) += devapc-mt6873.o
device-apc-mt6873-$(CONFIG_DEVAPC_ARCH_MULTI) += devapc-mtk-multi-ao.o
endif

ifneq ($(wildcard $(srctree)/$(src)/devapc-mt6879.c),)
obj-$(CONFIG_DEVAPC_MT6879) += device-apc-mt6879.o
device-apc-mt6879-$(CONFIG_DEVAPC_MT6879) += devapc-mt6879.o
device-apc-mt6879-$(CONFIG_DEVAPC_ARCH_MULTI) += devapc-mtk-multi-ao.o
endif

ifneq ($(wildcard $(srctree)/$(src)/devapc-mt6893.c),)
obj-$(CONFIG_DEVAPC_MT6893) += device-apc-mt6893.o
device-apc-mt6893-$(CONFIG_DEVAPC_MT6893) += devapc-mt6893.o
device-apc-mt6893-$(CONFIG_DEVAPC_ARCH_MULTI) += devapc-mtk-multi-ao.o
endif

ifneq ($(wildcard $(srctree)/$(src)/devapc-mt6895.c),)
obj-$(CONFIG_DEVAPC_MT6895) += device-apc-mt6895.o
device-apc-mt6895-$(CONFIG_DEVAPC_MT6895) += devapc-mt6895.o
device-apc-mt6895-$(CONFIG_DEVAPC_ARCH_MULTI) += devapc-mtk-multi-ao.o
endif

ifneq ($(wildcard $(srctree)/$(src)/devapc-mt6983.c),)
obj-$(CONFIG_DEVAPC_MT6983) += device-apc-mt6983.o
device-apc-mt6983-$(CONFIG_DEVAPC_MT6983) += devapc-mt6983.o
device-apc-mt6983-$(CONFIG_DEVAPC_ARCH_MULTI) += devapc-mtk-multi-ao.o
endif
