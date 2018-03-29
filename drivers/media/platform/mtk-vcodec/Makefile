obj-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk_vcodec_intr.o \
				       mtk_vcodec_util.o \
				       mtk_vcodec_enc_drv.o \
				       mtk_vcodec_enc.o \
				       mtk_vcodec_enc_pm.o \
				       venc_drv_if.o \
				       mtk_vcodec_dec_drv.o \
				       mtk_vcodec_dec.o \
				       mtk_vcodec_dec_pm.o \
				       vdec_drv_if.o \
				       mtkbuf-dma-cache-sg.o

obj-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += vp8_enc/ h264_enc/ h264_dec/ vp8_dec/ vp9_dec/ mpeg4_dec/

ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcodec
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vpu

ifeq ($(CONFIG_VIDEO_MEDIATEK_VCODEC),y)
    obj-$(CONFIG_MTK_IOMMU) += mtk_vcodec_iommu.o
endif

