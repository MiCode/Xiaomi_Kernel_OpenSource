subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq
mtk-vcu-y += mtk_vcu.o \
	mtk_vcodec_mem.o \

obj-$(CONFIG_VIDEO_MEDIATEK_VCU) += mtk-vcu.o
ccflags-y += -I$(srctree)/arch/arm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vcp/rv
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vcp/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/iommu
ccflags-y += -I$(srctree)/drivers/dma-buf/heaps
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox
