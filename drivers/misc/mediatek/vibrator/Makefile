obj-$(CONFIG_MTK_VIBRATOR) := vibrator_drv.o
ifeq (y, $(filter y, $(CONFIG_MACH_MT6885) $(CONFIG_MACH_MT6893)))
obj-y += $(subst ",,mt6885)/
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6853) $(CONFIG_MACH_MT6833) $(CONFIG_MACH_MT6877)))
obj-y += $(subst ",,mt6853)/
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6785) $(CONFIG_MACH_MT6781)))
obj-y += $(subst ",,mt6785)/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
ifeq (y, $(filter y, $(CONFIG_MACH_MT6885) $(CONFIG_MACH_MT6893)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vibrator/mt6885
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6853) $(CONFIG_MACH_MT6833) $(CONFIG_MACH_MT6877)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vibrator/mt6853
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6785) $(CONFIG_MACH_MT6781)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vibrator/mt6785
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vibrator/$(MTK_PLATFORM)
endif
