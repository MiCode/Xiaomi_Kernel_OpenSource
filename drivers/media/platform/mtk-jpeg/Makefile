mtk_jpeg-objs := mtk_jpeg_core.o \
	mtk_jpeg_enc_hw.o \
	mtk_jpeg_dec_hw.o \
	mtk_jpeg_dec_parse.o
obj-y += mtk_jpeg.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi


ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt6781/include
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
endif

ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/mt6781
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion
ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion/mtk
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs