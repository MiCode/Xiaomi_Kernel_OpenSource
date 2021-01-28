ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/staging/android/ion/mtk
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(srctree)/drivers/iommu
ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/

FDVT_35_PLATFORM = mt6763 mt6761
FDVT_40_PLATFORM = mt6758 mt6799 mt6759 mt6775 mt6771 mt6765
FDVT_50_PLATFORM = mt6779
FDVT_CURRENT_PLATFORM = $(subst ",,$(MTK_PLATFORM))

ifneq (, $(findstring $(FDVT_CURRENT_PLATFORM), $(FDVT_35_PLATFORM)))
$(info FDVT: Drv use 3.5 folder)
MTKCAM_FDVT_VERSION := 3.5
else ifneq (, $(findstring $(FDVT_CURRENT_PLATFORM), $(FDVT_40_PLATFORM)))
$(info FDVT: Drv use 4.0 folder)
MTKCAM_FDVT_VERSION := 4.0
else ifneq (, $(findstring $(FDVT_CURRENT_PLATFORM), $(FDVT_50_PLATFORM)))
$(info FDVT: Drv use 5.0 folder)
MTKCAM_FDVT_VERSION := 5.0
endif

ifeq ($(CONFIG_MACH_MT6763),y)
  MTK_FD_LARB := 1
else ifeq ($(CONFIG_MACH_MT6761),y)
  MTK_FD_LARB := 2
endif
subdir-ccflags-y += -DMTK_FD_LARB=$(MTK_FD_LARB)

obj-$(CONFIG_MTK_CAMERA_ISP_FD_SUPPORT) += camera_fdvt.o
camera_fdvt-objs += $(MTKCAM_FDVT_VERSION)/camera_fdvt.o

