obj-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-dec.o \
				       mtk-vcodec-enc.o \
				       mtk-vcodec-common.o


mtk-vcodec-dec-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk_vcodec_dec_drv.o \
		vdec_drv_if.o \
		mtk_vcodec_dec.o \
		mtk_vcodec_dec_pm.o

mtk-vcodec-dec-$(CONFIG_VIDEO_MEDIATEK_VCU) += vdec/vdec_common_if.o \
		vdec_vcu_if.o

mtk-vcodec-$(CONFIG_VIDEO_MEDIATEK_VPU) += vdec/vdec_h264_if.o \
		vdec/vdec_vp8_if.o \
		vdec/vdec_vp9_if.o \
		vdec_vpu_if.o

mtk-vcodec-enc-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk_vcodec_enc.o \
		mtk_vcodec_enc_drv.o \
		mtk_vcodec_enc_pm.o \
		venc_drv_if.o

mtk-vcodec-enc-$(CONFIG_VIDEO_MEDIATEK_VCU) += venc/venc_common_if.o \
		venc_vcu_if.o

mtk-vcodec-$(CONFIG_VIDEO_MEDIATEK_VPU) += venc/venc_vp8_if.o \
		venc/venc_h264_if.o \
		venc_vpu_if.o

mtk-vcodec-common-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk_vcodec_intr.o \
		mtk_vcodec_util.o\
		vcodec_dvfs.o \
		vcodec_bw.o

subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcu
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcodec
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-videobuf
ccflags-y += -I$(srctree)/drivers/misc/mediatek/slbc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(srctree)/arch/arm
ccflags-y += -I$(srctree)/drivers/staging/android/ion
