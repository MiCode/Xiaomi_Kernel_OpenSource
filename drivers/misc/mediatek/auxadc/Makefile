ifeq (y, $(filter y, $(CONFIG_MACH_MT6761)))
MTK_PLATFORM:= $(subst ",,mt6765)
else
MTK_PLATFORM:= $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/auxadc/$(MTK_PLATFORM)

obj-$(CONFIG_MTK_AUXADC) += mtk_auxadc.o

