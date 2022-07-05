# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -I$(srctree)/drivers/clk/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vcp/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vcp/rv/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs
obj-$(CONFIG_MTK_CMDQ) += mtk-cmdq-helper.o
obj-$(CONFIG_MTK_DVFSRC) += mtk-dvfsrc.o mtk-dvfsrc-start.o
obj-$(CONFIG_MTK_IPI) += mtk_tinysys_ipi.o
obj-$(CONFIG_MTK_MBOX) += mtk-mbox.o
obj-$(CONFIG_MTK_DEVAPC) += mtk-devapc.o
obj-$(CONFIG_MTK_PMIC_WRAP) += mtk-pmic-wrap.o
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys.o mtk-pm-domain-disable-unused.o
ifneq ($(wildcard $(srctree)/$(src)/mtk-scpsys-mt6886.c),)
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys-mt6886.o mtk-scpsys-bringup.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk-scpsys-mt6983.c),)
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys-mt6983.o mtk-scpsys-bringup.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk-scpsys-mt6985.c),)
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys-mt6985.o mtk-scpsys-bringup.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mt8188-pm-domains.h),)
obj-$(CONFIG_MTK_SCPSYS_PM_DOMAINS) += mtk-pm-domains.o
endif
obj-$(CONFIG_MTK_MMSYS) += mtk-mmsys.o
obj-$(CONFIG_MTK_DEVAPC) += devapc/
obj-$(CONFIG_MTK_DEVMPU) += devmpu/
obj-$(CONFIG_MTK_MMDVFS) += mtk-mmdvfs.o mtk-mmdvfs-v3.o
obj-$(CONFIG_MTK_MMSYS) += mtk-mutex.o
obj-$(CONFIG_MTK_SOCINFO) += mtk-socinfo.o
