subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)
mtk-vcu-y += mtk_vcu.o \
	mtk_vcodec_mem.o \

obj-$(CONFIG_VIDEO_MEDIATEK_VCU) += mtk-vcu.o mtk_vcodec_mem.o
ccflags-y += -I$(srctree)/arch/arm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
