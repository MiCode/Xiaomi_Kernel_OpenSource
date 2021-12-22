#$(error "adsfasdf")
MCUPM_VERSION := v1
MCUPM_VERSION_PATH := ../$(MCUPM_VERSION)

obj-y += $(MCUPM_VERSION_PATH)/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MCUPM_VERSION)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDEBUG
