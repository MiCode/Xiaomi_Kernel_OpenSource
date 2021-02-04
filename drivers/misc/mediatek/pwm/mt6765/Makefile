ifeq (y, $(filter y, $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761)))
PWM_PLAT_NAME = $(subst ",,mt6765)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pwm/mt6765/include
else
PWM_PLAT_NAME = $(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pwm/$(PWM_PLAT_NAME)/include
endif

obj-$(CONFIG_MTK_PWM)	:= mtk_pwm_hal.o
