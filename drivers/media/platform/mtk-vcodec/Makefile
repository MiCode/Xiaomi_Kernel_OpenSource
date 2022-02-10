ifeq (y, $(filter y, $(CONFIG_MACH_MT6781)))
MTK_PLATFORM := mt6781
else
MTK_PLATFORM := $(CONFIG_MTK_PLATFORM:"%"=%)
endif

ifeq (y, $(filter y, $(CONFIG_MACH_MT6893)))
MTK_PLATFORM := mt6885
endif
ifeq (y, $(filter y, $(CONFIG_MACH_MT6833)))
MTK_PLATFORM := mt6833
endif
ifeq (y, $(filter y, $(CONFIG_MACH_MT6877)))
MTK_PLATFORM := mt6877
endif
ifeq (y, $(filter y, $(CONFIG_MACH_MT6781)))
MTK_PLATFORM := mt6781
endif

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
		vdec/vdec_log_if.o \
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
		venc/venc_log_if.o \
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
ifdef CONFIG_MTK_PSEUDO_M4U
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
endif
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcodec
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcodec/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-videobuf
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/variant
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmqos
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(srctree)/arch/arm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs
ccflags-y += -I$(srctree)/drivers/misc/mediatek/slbc
ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion
ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion/mtk

