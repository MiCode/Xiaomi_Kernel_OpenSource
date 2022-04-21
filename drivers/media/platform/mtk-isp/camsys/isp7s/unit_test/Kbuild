ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp \
	     -I$(srctree)/drivers/media/platform/mtk-isp/camsys/isp7_1 \
		 -I$(srctree)/drivers/rpmsg/ \
		 -I$(srctree)/drivers/remoteproc/

cam-ut-objs := mtk_cam_ut-cam.o \
	       mtk_cam_ut-seninf.o \
	       mtk_cam_ut.o \
	       mtk_cam_ut-camsv.o

obj-$(CONFIG_VIDEO_MTK_ISP_CAMSYS) += cam-ut.o
