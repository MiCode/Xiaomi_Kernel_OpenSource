mtk_jpeg-objs := mtk_jpeg_core.o \
	mtk_jpeg_enc_hw.o \
	mtk_jpeg_dec_hw.o \
	mtk_jpeg_dec_parse.o
obj-y += mtk_jpeg.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/staging/android/ion/mtk
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs