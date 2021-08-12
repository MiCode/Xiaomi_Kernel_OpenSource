# SPDX-License-Identifier: GPL-2.0-only

subdir-ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp \
	     -I$(srctree)/drivers/misc/mediatek/mmdvfs \
	     -I$(srctree)/drivers/misc/mediatek/smi/ \
	     -I$(srctree)/drivers/misc/mediatek/include/mt-plat/ \
	     -I$(srctree)/drivers/misc/mediatek/mtk-interconnect \
	     -I$(srctree)/drivers/misc/mediatek/imgsensor/src-v4l2/frame-sync/ \
	     -I$(srctree)/drivers/misc/mediatek/imgsensor/inc/

MTK_CAMSYS_DRIVER_PATH := $(srctree)/drivers/media/platform/mtk-isp/camsys/isp7_1/cam

mtk-cam-isp-objs := mtk_cam.o mtk_cam-raw.o mtk_cam-pool.o mtk_cam_pm.o\
		    mtk_cam-video.o mtk_cam-smem.o \
		    mtk_cam-ctrl.o \
		    mtk_cam-seninf-route.o mtk_cam-seninf-drv.o \
		    mtk_cam-dvfs_qos.o \
		    mtk_cam-debug.o \
		    mtk_cam-sv.o \
		    mtk_cam-raw_debug.o

include $(MTK_CAMSYS_DRIVER_PATH)/mtk_csi_phy_2_0/sub_drv.mk
include $(MTK_CAMSYS_DRIVER_PATH)/mtk_csi_phy_3_0/sub_drv.mk
#ifeq ($(CONFIG_DEBUG_FS), y)
#mtk-cam-isp-objs += mtk_cam-procdbg.o
#endif

obj-$(CONFIG_VIDEO_MTK_ISP_CAMSYS) += mtk-cam-isp.o
