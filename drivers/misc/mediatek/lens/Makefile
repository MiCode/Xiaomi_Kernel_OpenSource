#
# Makefile for misc devices that really don't fit anywhere else.
#

obj-$(CONFIG_MTK_LENS)   += mtk/main/
obj-$(CONFIG_MTK_LENS)   += mtk/main2/
ifeq ($(CONFIG_MACH_MT6761), y)
	ifeq ($(CONFIG_EXTREME_LOW_RAM), y)
	else
	obj-$(CONFIG_MTK_LENS)   += mtk/main3/
	obj-$(CONFIG_MTK_LENS)   += mtk/sub2/
	endif
else
obj-$(CONFIG_MTK_LENS)   += mtk/main3/
obj-$(CONFIG_MTK_LENS)   += mtk/sub2/
endif
obj-$(CONFIG_MTK_LENS)   += mtk/sub/
