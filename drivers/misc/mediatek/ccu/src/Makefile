# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2016 MediaTek Inc.
#

subdir-ccflags-y += -DMTK_VPU_KERNEL
subdir-ccflags-y += -DCCU_AF_ENABLE

ifeq ($(CONFIG_MTK_CCU),m)
obj-$(CONFIG_MTK_CCU) += isp4/
obj-$(CONFIG_MTK_CCU) += isp6s/
else
obj-$(CONFIG_MTK_CCU_ISP4) += isp4/
obj-$(CONFIG_MTK_CCU_ISP6S) += isp6s/
endif
