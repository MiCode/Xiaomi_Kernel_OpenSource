
MTK_CUSTOM_PATH  := $(srctree)/drivers/misc/mediatek

MTKCAM_FDVT_VERSION := 5.1
MTKCAM_FDVT_PATH := $(MTK_CUSTOM_PATH)/cameraisp/fdvt/5.1
subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/cameraisp/fdvt/5.1/inc
$(info fdvt tina)

#ifdef CONFIG_MTK_M4U
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
#else
ccflags-y += -I$(srctree)/drivers/iommu/
#endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/dma-buf/heaps/
ccflags-y += -I$(srctree)/drivers/staging/android/ion \
	     -I$(srctree)/drivers/staging/android/ion/mtk

$(info Tina add dma: $(CONFIG_MTK_CAM_SECURE_FD_SUPPORT))
ifeq ($(strip $(CONFIG_MTK_CAM_SECURE_FD_SUPPORT)),y)
ccflags-y += -DCMDQ_MTEE
endif

#obj-y:= fdvt_dummy.o
obj-$(CONFIG_MTK_CAMERA_FD_ISP6S) += camera_fdvt_isp51.o
camera_fdvt_isp51-objs += 5.1/camera_fdvt.o

ifeq ($(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT)),y)
  subdir-ccflags-y += -DMTK_SECURE_FD_SUPPORT
endif
