
obj-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-dec.o \
				       mtk-vcodec-enc.o \
				       mtk-vcodec-common.o \
				       $(MTK_PLATFORM)/vcodec_dvfs.o \
				       $(MTK_PLATFORM)/vcodec_bw.o

mtk-vcodec-dec-y := mtk_vcodec_dec_drv.o \
		vdec_drv_if.o \
		mtk_vcodec_dec.o \
		$(MTK_PLATFORM)/mtk_vcodec_dec_pm.o \

ifdef CONFIG_VIDEO_MEDIATEK_VCU
	mtk-vcodec-dec-y += vdec/vdec_common_if.o \
		vdec_vcu_if.o
endif
ifdef CONFIG_VIDEO_MEDIATEK_VPU
	mtk-vcodec-dec-y += vdec/vdec_h264_if.o \
		vdec/vdec_vp8_if.o \
		vdec/vdec_vp9_if.o \
		vdec_vpu_if.o
endif

mtk-vcodec-enc-y := mtk_vcodec_enc.o \
		mtk_vcodec_enc_drv.o \
		$(MTK_PLATFORM)/mtk_vcodec_enc_pm.o \
		venc_drv_if.o

ifdef CONFIG_VIDEO_MEDIATEK_VCU
	mtk-vcodec-enc-y += venc/venc_common_if.o \
		venc_vcu_if.o
endif
ifdef CONFIG_VIDEO_MEDIATEK_VPU
	mtk-vcodec-enc-y += venc/venc_vp8_if.o \
		venc/venc_h264_if.o \
		venc_vpu_if.o
endif

mtk-vcodec-common-y := $(MTK_PLATFORM)/mtk_vcodec_intr.o \
		mtk_vcodec_util.o\

subdir-ccflags-y += -Werror
ifdef CONFIG_VIDEO_MEDIATEK_VCU
	ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcu
endif
ifdef CONFIG_VIDEO_MEDIATEK_VPU
	ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vpu
endif
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcodec
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcodec/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-videobuf
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/variant
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
