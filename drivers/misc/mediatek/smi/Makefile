# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/iommu

obj-$(CONFIG_MTK_SMI) += mtk-smi-dbg.o
obj-$(CONFIG_MTK_SMI_BWC) += mtk-smi-bwc.o

mtk-smi-bwc-objs += mtk-smi-bwc-main.o

ifneq ($(filter y m,$(CONFIG_MTK_SMI_BWC_MT6739)),)
ccflags-y += -DSMI_ZIO
ccflags-y += -DMMDVFS_HOOK
ccflags-y += -DVCORE_READY
ccflags-y += -I$(srctree)/drivers/clk/mediatek
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/vcorefs_v3/
mtk-smi-bwc-objs += mmdvfs_mgr_v3.o mmdvfs_profile_adaptor.o mmdvfs_mgr_util.o
else ifneq ($(filter y m,$(CONFIG_MTK_SMI_BWC_MT6761)),)
ccflags-y += -DSMI_MER
ccflags-y += -DMMDVFS_HOOK
ccflags-y += -DMMDVFS_QOS_SUPPORT
mtk-smi-bwc-objs += mmdvfs_mgr_v3.o mmdvfs_profile_adaptor.o mmdvfs_mgr_util.o
else ifneq ($(filter y m,$(CONFIG_MTK_SMI_BWC_MT6765)),)
ccflags-y += -DSMI_CER
ccflags-y += -DMMDVFS_HOOK
ccflags-y += -DMMDVFS_QOS_SUPPORT
mtk-smi-bwc-objs += mmdvfs_mgr_v3.o mmdvfs_profile_adaptor.o mmdvfs_mgr_util.o
endif
