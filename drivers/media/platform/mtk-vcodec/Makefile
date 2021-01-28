# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-dec.o \
				       mtk-vcodec-enc.o \
				       mtk-vcodec-common.o \

mtk-vcodec-dec-y := mtk_vcodec_dec_drv.o \
		vdec_drv_if.o \
		mtk_vcodec_dec.o \
		mtk_vcodec_dec_stateful.o \
		mtk_vcodec_dec_stateless.o \
		mtk_vcodec_dec_pm.o \
		mtk_vcodec_fw.o \
		vcodec_dvfs.o \

ifdef CONFIG_VIDEO_MEDIATEK_VCU
	mtk-vcodec-dec-y += vdec/vdec_common_if.o \
		vdec_vcu_if.o
endif
ifdef CONFIG_VIDEO_MEDIATEK_VPU
	mtk-vcodec-dec-y += vdec/vdec_h264_if.o \
		vdec/vdec_h264_req_if.o \
		vdec/vdec_vp8_if.o \
		vdec/vdec_vp9_if.o \
		vdec_vpu_if.o
endif

mtk-vcodec-enc-y := mtk_vcodec_enc.o \
		mtk_vcodec_enc_drv.o \
		mtk_vcodec_enc_pm.o \
		venc_drv_if.o \

ifdef CONFIG_VIDEO_MEDIATEK_VCU
	mtk-vcodec-enc-y += venc/venc_common_if.o \
		venc_vcu_if.o
endif
ifdef CONFIG_VIDEO_MEDIATEK_VPU
	mtk-vcodec-enc-y += venc/venc_vp8_if.o \
		venc/venc_h264_if.o \
		venc_vpu_if.o
endif


mtk-vcodec-common-y := mtk_vcodec_intr.o \
		mtk_vcodec_util.o\

ifdef CONFIG_VIDEO_MEDIATEK_VCU
	ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcu
endif
ifdef CONFIG_VIDEO_MEDIATEK_VPU
	ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vpu
endif
ifdef CONFIG_VB2_MEDIATEK_DMA
	ccflags-y += -I$(srctree)/drivers/media/platform/mtk-videobuf
endif
