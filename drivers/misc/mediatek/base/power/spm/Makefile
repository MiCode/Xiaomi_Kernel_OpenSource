subdir-ccflags-y += -DSECURE_SERIAL_8250

ifeq (y, $(filter y, $(CONFIG_MACH_MT6768) $(CONFIG_MACH_MT6785)))
obj-y += common_v1/
else
obj-y += common/
endif
obj-y += $(MTK_PLATFORM)/
