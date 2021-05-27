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


ifeq ($(CONFIG_VIDEO_MEDIATEK_VCODEC_MT6885),y)
	obj-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-dec-mt6885.o \
	                       mtk-vcodec-enc-mt6885.o

	mtk-vcodec-dec-mt6885-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += \
			v2/mtk_vcodec_dec_pm_plat.o \
			v2/vcodec_dvfs.o \
			v2/vcodec_bw.o

	mtk-vcodec-enc-mt6885-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += \
			v2/mtk_vcodec_enc_pm_plat.o \
			v2/vcodec_dvfs.o \
			v2/vcodec_bw.o

	mtk-vcodec-dec-mt6885-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-dec.o
	mtk-vcodec-enc-mt6885-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-enc.o
endif

ifeq ($(CONFIG_VIDEO_MEDIATEK_VCODEC_MT6983),y)
	obj-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-dec-mt6983.o \
	                       mtk-vcodec-enc-mt6983.o

	mtk-vcodec-dec-mt6983-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += \
			v2/mtk_vcodec_dec_pm_plat.o \
			v2/vcodec_dvfs.o \
			v2/vcodec_bw.o

	mtk-vcodec-enc-mt6983-$(CONFIG_VIDEO_MEDIATEK_VCODEC) += \
			v2/mtk_vcodec_enc_pm_plat.o \
			v2/vcodec_dvfs.o \
			v2/vcodec_bw.o

	mtk-vcodec-dec-mt6983$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-dec.o
	mtk-vcodec-enc-mt6983$(CONFIG_VIDEO_MEDIATEK_VCODEC) += mtk-vcodec-enc.o
endif


subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcu
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcodec
ifeq ($(CONFIG_VIDEO_MEDIATEK_VCODEC_MT6885),y)
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcodec/v2
endif
ifeq ($(CONFIG_VIDEO_MEDIATEK_VCODEC_MT6983),y)
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcodec/v2
endif
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-videobuf
ccflags-y += -I$(srctree)/drivers/misc/mediatek/slbc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/iommu
ccflags-y += -I$(srctree)/arch/arm
ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vcp/rv
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vcp/include

#endif
