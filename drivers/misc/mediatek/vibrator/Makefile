obj-$(CONFIG_MTK_VIBRATOR) := vibrator_drv.o

ifeq (y, $(filter y, $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vibrator/mt6765
obj-y += $(subst ",,mt6765)/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vibrator/$(MTK_PLATFORM)
endif
