# UFSHCD makefile
obj-$(CONFIG_SCSI_UFS_DWC_TC_PCI) += tc-dwc-g210-pci.o ufshcd-dwc.o tc-dwc-g210.o
obj-$(CONFIG_SCSI_UFS_DWC_TC_PLATFORM) += tc-dwc-g210-pltfrm.o ufshcd-dwc.o tc-dwc-g210.o

ifeq ($(CONFIG_MTK_UFS_SUPPORT),y)
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
MTK_MISC_INC := drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/$(MTK_MISC_INC)/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/$(MTK_MISC_INC)/mt-plat
ccflags-y += -I$(srctree)/$(MTK_MISC_INC)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/scsi/ufs/mediatek/$(MTK_PLATFORM)
endif

obj-$(CONFIG_SCSI_UFS_QCOM) += ufs-qcom.o
obj-$(CONFIG_SCSI_UFSHCD) += ufshcd.o
obj-$(CONFIG_UFSHPB) += ufshpb.o
obj-$(CONFIG_SCSI_UFSHCD_PCI) += ufshcd-pci.o
obj-$(CONFIG_SCSI_UFSHCD_PLATFORM) += ufshcd-pltfrm.o
obj-$(CONFIG_MTK_UFS_SUPPORT) += ufs-mtk.o
obj-$(CONFIG_MTK_UFS_SUPPORT) += mediatek/$(MTK_PLATFORM)/ufs-mtk-platform.o
obj-$(CONFIG_MTK_UFS_BLOCK_IO_LOG) += ufs-mtk-block.o
obj-$(CONFIG_MTK_UFS_SUPPORT) += ufs-mtk-dbg.o
