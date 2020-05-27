# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

obj-$(CONFIG_MTK_DEVAPC) := devapc.o

# Core
devapc-$(CONFIG_DEVAPC_ARCH_MULTI) += devapc-mtk-multi-ao.o

# Platform
devapc-$(CONFIG_DEVAPC_MT6873) += devapc-mt6873.o
