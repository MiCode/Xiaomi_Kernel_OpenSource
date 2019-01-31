ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/cpuidle_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/

obj-y += mtk_cpuidle.o

obj-$(CONFIG_MACH_MT6757) += mtk_cpuidle_mt6757.o
obj-$(CONFIG_MACH_MT6799) += mtk_cpuidle_mt6799.o
obj-$(CONFIG_MACH_MT6759) += mtk_cpuidle_mt6759.o
obj-$(CONFIG_MACH_MT6763) += mtk_cpuidle_mt6763.o
obj-$(CONFIG_MACH_MT6758) += mtk_cpuidle_mt6758.o
