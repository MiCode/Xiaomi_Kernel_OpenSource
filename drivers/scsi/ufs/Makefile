# SPDX-License-Identifier: GPL-2.0
# UFSHCD makefile
ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE_ufs-mtk.o := y
GCOV_PROFILE_ufs-mtk-platform.o := y
GCOV_PROFILE_ufs-mtk-dbg.o := y
GCOV_PROFILE_ufs-mtk-block.o := y
endif
obj-$(CONFIG_SCSI_UFS_DWC_TC_PCI) += tc-dwc-g210-pci.o ufshcd-dwc.o tc-dwc-g210.o
obj-$(CONFIG_SCSI_UFS_DWC_TC_PLATFORM) += tc-dwc-g210-pltfrm.o ufshcd-dwc.o tc-dwc-g210.o

ifeq ($(CONFIG_MTK_UFS_SUPPORT),y)
ifeq (y, $(filter y, $(CONFIG_MACH_MT6893)))
MTK_PLATFORM := mt6893
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif
MTK_MISC_INC := drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/$(MTK_MISC_INC)/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/$(MTK_MISC_INC)/mt-plat
ccflags-y += -I$(srctree)/$(MTK_MISC_INC)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/scsi/ufs/mediatek/$(MTK_PLATFORM)
endif

obj-$(CONFIG_SCSI_UFS_QCOM) += ufs-qcom.o
obj-$(CONFIG_SCSI_UFSHCD) += ufshcd-core.o
ufshcd-core-y := ufshcd.o
ufshcd-core-$(CONFIG_SCSI_UFS_CRYPTO) += ufshcd-crypto.o
obj-$(CONFIG_UFSFEATURE) += ufsfeature.o
obj-$(CONFIG_UFSHPB) += ufshpb.o
obj-$(CONFIG_UFSTW) += ufstw.o
obj-$(CONFIG_SCSI_SKHPB) += ufshpb_skh.o
obj-$(CONFIG_SCSI_UFSHCD_PCI) += ufshcd-pci.o
obj-$(CONFIG_SCSI_UFSHCD_PLATFORM) += ufshcd-pltfrm.o
obj-$(CONFIG_MTK_UFS_SUPPORT) += ufs-mtk.o
obj-$(CONFIG_MTK_UFS_SUPPORT) += mediatek/$(MTK_PLATFORM)/ufs-mtk-platform.o
obj-$(CONFIG_MTK_UFS_BLOCK_IO_LOG) += ufs-mtk-block.o
obj-$(CONFIG_MTK_UFS_SUPPORT) += ufs-mtk-dbg.o