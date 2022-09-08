# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox
obj-$(CONFIG_MTK_CMDQ) += mtk-cmdq-helper.o
obj-$(CONFIG_MTK_DVFSRC) += mtk-dvfsrc.o mtk-dvfsrc-start.o
obj-$(CONFIG_MTK_IPI) += mtk_tinysys_ipi.o
obj-$(CONFIG_MTK_MBOX) += mtk-mbox.o
obj-$(CONFIG_MTK_PMIC_WRAP) += mtk-pmic-wrap.o
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys.o mtk-pm-domain-disable-unused.o

ifneq ($(wildcard $(srctree)/$(src)/mtk-scpsys-mt6768.c),)
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys-mt6768.o mtk-scpsys-bringup.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk-scpsys-mt6765.c),)
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys-mt6765.o mtk-scpsys-bringup.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk-scpsys-mt6789.c),)
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys-mt6789.o mtk-scpsys-bringup.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk-scpsys-mt6833.c),)
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys-mt6833.o mtk-scpsys-bringup.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk-scpsys-mt6855.c),)
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys-mt6855.o mtk-scpsys-bringup.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk-scpsys-mt6893.c),)
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys-mt6893.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk-scpsys-mt6895.c),)
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys-mt6895.o mtk-scpsys-bringup.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk-scpsys-mt6879.c),)
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys-mt6879.o mtk-scpsys-bringup.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk-scpsys-mt6983.c),)
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys-mt6983.o mtk-scpsys-bringup.o
endif
obj-$(CONFIG_MTK_MMSYS) += mtk-mmsys.o
obj-$(CONFIG_MTK_DEVAPC) += devapc/
obj-$(CONFIG_MTK_DEVMPU) += devmpu/
obj-$(CONFIG_MTK_MMDVFS) += mtk-mmdvfs.o
obj-$(CONFIG_MTK_SOCINFO) += mtk-socinfo.o
