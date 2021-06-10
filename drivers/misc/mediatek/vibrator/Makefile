obj-$(CONFIG_MTK_VIBRATOR) := vibrator_drv.o
ifeq (y, $(filter y, $(CONFIG_MACH_MT6885) $(CONFIG_MACH_MT6893)))
obj-y += $(subst ",,mt6885)/
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6853) $(CONFIG_MACH_MT6833)))
obj-y += $(subst ",,mt6853)/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
ifeq (y, $(filter y, $(CONFIG_MACH_MT6885) $(CONFIG_MACH_MT6893)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vibrator/mt6885
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6853) $(CONFIG_MACH_MT6833)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vibrator/mt6853
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vibrator/$(MTK_PLATFORM)
endif
