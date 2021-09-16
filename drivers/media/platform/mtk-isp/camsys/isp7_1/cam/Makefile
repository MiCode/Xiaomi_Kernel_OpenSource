# SPDX-License-Identifier: GPL-2.0-only

subdir-ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp \
	     -I$(srctree)/drivers/misc/mediatek/mmdvfs \
	     -I$(srctree)/drivers/misc/mediatek/smi/ \
	     -I$(srctree)/drivers/misc/mediatek/include/mt-plat/ \
	     -I$(srctree)/drivers/misc/mediatek/mtk-interconnect \
	     -I$(srctree)/drivers/misc/mediatek/imgsensor/src-v4l2/frame-sync/ \
	     -I$(srctree)/drivers/misc/mediatek/imgsensor/inc/ \
	     -I$(srctree)/drivers/dma-buf/heaps/

MTK_CAMSYS_DRIVER_PATH := $(srctree)/drivers/media/platform/mtk-isp/camsys/isp7_1/cam
# MTEE part
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/m4u/
ccflags-y += -I$(srctree)/drivers/dma-buf/heaps
# MTEE end

ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -DSENSOR_SECURE_MTEE_SUPPORT
ccflags-y += -I$(srctree)/drivers/misc/mediatek/tee_gp/include
# ccflags-y += -DSECURE_UT

mtk-cam-isp-objs := mtk_cam.o mtk_cam-raw.o mtk_cam-pool.o mtk_cam_pm.o\
		    mtk_cam-video.o mtk_cam-smem.o mtk_cam_vb2-dma-contig.o\
		    mtk_cam-ctrl.o \
		    mtk_cam-seninf-route.o mtk_cam-seninf-drv.o \
		    mtk_cam-dvfs_qos.o \
		    mtk_cam-debug.o \
		    mtk_cam-sv.o \
		    mtk_cam-raw_debug.o \
		    mtk_cam-tg-flash.o \
		    mtk_cam-feature.o mtk_cam-feature-m2m.o \
		    mtk_cam-feature-mstream.o mtk_cam-feature-smvr.o \
		    mtk_cam-feature-stagger.o \
		    mtk_cam-mraw.o\
		    mtk_cam-seninf-ca.o

#mtk-cam-isp-$(CONFIG_TRACING) += mtk_cam-trace.o

mtk-cam-isp-objs +=  mtk_cam-hsf.o
#mtk-cam-isp-objs += mtk_cam-procdbg.o

include $(MTK_CAMSYS_DRIVER_PATH)/mtk_csi_phy_2_0/sub_drv.mk
include $(MTK_CAMSYS_DRIVER_PATH)/mtk_csi_phy_3_0/sub_drv.mk

obj-$(CONFIG_VIDEO_MTK_ISP_CAMSYS) += mtk-cam-isp.o
