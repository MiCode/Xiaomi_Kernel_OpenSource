subdir-ccflags-y += -Werror
mtk-vcu-y += mtk_vcu.o \
	mtk_vcodec_mem.o \

obj-$(CONFIG_VIDEO_MEDIATEK_VCU) += mtk-vcu.o mtk_vcodec_mem.o
ccflags-y += -I$(srctree)/arch/arm

ifdef CONFIG_VB2_MEDIATEK_DMA
	ccflags-y += -I$(srctree)/drivers/media/platform/mtk-videobuf
endif
