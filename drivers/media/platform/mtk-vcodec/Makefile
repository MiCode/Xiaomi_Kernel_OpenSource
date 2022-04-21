#ifneq ($(MTK_VCODEC_PLATFORM),)

obj-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-common.o

mtk-vcodec-dec-$(CONFIG_VIDEO_MEDIATEK_VCU) += vdec/vdec_common_if.o \
		vdec/vdec_vcu_if.o

mtk-vcodec-enc-$(CONFIG_VIDEO_MEDIATEK_VCU) += venc/venc_common_if.o \
		venc/venc_vcu_if.o

mtk-vcodec-dec-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += vdec/vdec_vcp_if.o
mtk-vcodec-enc-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += venc/venc_vcp_if.o

mtk-vcodec-dec-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk_vcodec_dec_drv.o \
		vdec_drv_if.o \
		mtk_vcodec_dec.o \
		mtk_vcodec_dec_pm.o

mtk-vcodec-enc-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk_vcodec_enc_drv.o \
		venc_drv_if.o \
		mtk_vcodec_enc.o \
		mtk_vcodec_enc_pm.o

mtk-vcodec-common-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk_vcodec_intr.o \
		mtk_vcodec_util.o

ifeq ($(CONFIG_VIDEO_MEDIATEK_VCODEC_V1),y)
	obj-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-dec-v1.o \
	                       mtk-vcodec-enc-v1.o

	mtk-vcodec-dec-v1-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += \
			v1/mtk_vcodec_dec_pm_plat.o \
			v1/vcodec_dvfs.o

	mtk-vcodec-enc-v1-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += \
			v1/mtk_vcodec_enc_pm_plat.o \
			v1/vcodec_dvfs.o

	mtk-vcodec-dec-v1-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-dec.o
	mtk-vcodec-enc-v1-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-enc.o
endif

ifeq ($(CONFIG_VIDEO_MEDIATEK_VCODEC_V2),y)
	obj-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-dec-v2.o \
	                       mtk-vcodec-enc-v2.o

	mtk-vcodec-dec-v2-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += \
			v2/mtk_vcodec_dec_pm_plat.o \
			v2/vcodec_dvfs.o

	mtk-vcodec-enc-v2-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += \
			v2/mtk_vcodec_enc_pm_plat.o \
			v2/vcodec_dvfs.o

	mtk-vcodec-dec-v2-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-dec.o
	mtk-vcodec-enc-v2-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-enc.o
endif

subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcu
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcodec
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-videobuf
ccflags-y += -I$(srctree)/drivers/misc/mediatek/slbc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/iommu
ccflags-y += -I$(srctree)/drivers/iommu
ccflags-y += -I$(srctree)/arch/arm
ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vcp/rv
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vcp/include
ccflags-y += -I$(srctree)/drivers/dma-buf/heaps
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/

#endif
