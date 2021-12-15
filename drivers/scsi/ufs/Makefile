# SPDX-License-Identifier: GPL-2.0
# UFSHCD makefile

MTK_MISC_INC := drivers/misc/mediatek/include

ifeq ($(CONFIG_SCSI_UFS_MEDIATEK),y)

MTK_MISC_INC := drivers/misc/mediatek/include
ifeq (y, $(filter y, $(CONFIG_MACH_MT6893)))
MTK_PLATFORM := mt6893
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6877)))
MTK_PLATFORM := mt6877
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6781)))
MTK_PLATFORM := mt6781
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif


ccflags-y += -I$(srctree)/$(MTK_MISC_INC)
ccflags-y += -I$(srctree)/$(MTK_MISC_INC)/mt-plat
ccflags-y += -I$(srctree)/$(MTK_MISC_INC)/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
endif

obj-$(CONFIG_SCSI_UFS_DWC_TC_PCI) += tc-dwc-g210-pci.o ufshcd-dwc.o tc-dwc-g210.o
obj-$(CONFIG_SCSI_UFS_DWC_TC_PLATFORM) += tc-dwc-g210-pltfrm.o ufshcd-dwc.o tc-dwc-g210.o
obj-$(CONFIG_SCSI_UFS_QCOM) += ufs-qcom.o
obj-$(CONFIG_SCSI_UFSHCD) += ufshcd-core.o
ufshcd-core-objs := ufshcd.o ufs-sysfs.o
ufshcd-core-$(CONFIG_SCSI_UFS_BSG)	+= ufs_bsg.o
obj-$(CONFIG_SCSI_UFS_FEATURE) += ufsfeature.o
obj-$(CONFIG_SCSI_UFS_HPB) += ufshpb.o
obj-$(CONFIG_SCSI_UFS_TW) += ufstw.o
obj-$(CONFIG_SCSI_SKHPB) += ufshpb_skh.o
obj-$(CONFIG_SCSI_UFSHCD_PCI) += ufshcd-pci.o
obj-$(CONFIG_SCSI_UFSHCD_PLATFORM) += ufshcd-pltfrm.o
obj-$(CONFIG_SCSI_UFS_HISI) += ufs-hisi.o
obj-$(CONFIG_SCSI_UFS_MEDIATEK) += ufs-mediatek.o ufs-mediatek-dbg.o
ufshcd-core-$(CONFIG_SCSI_UFS_CRYPTO) += ufshcd-crypto.o
obj-$(CONFIG_MTK_UFS_BLOCK_IO_LOG) += ufs-mtk-block.o
