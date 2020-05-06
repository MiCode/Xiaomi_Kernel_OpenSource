# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/isp_60/cam \
             -I$(srctree)/drivers/misc/mediatek/mmdvfs \
	     -I$(srctree)/drivers/misc/mediatek/smi/

mtk-cam-isp-objs := mtk_cam.o mtk_cam-raw.o mtk_cam-pool.o \
		    mtk_cam-video.o mtk_cam-smem.o \
		    mtk_cam-ctrl.o mtk_cam-seninf-hw.o \
		    mtk_cam-seninf-route.o mtk_cam-seninf-drv.o

obj-$(CONFIG_VIDEO_MTK_ISP_CAMSYS) += mtk-cam-isp.o
