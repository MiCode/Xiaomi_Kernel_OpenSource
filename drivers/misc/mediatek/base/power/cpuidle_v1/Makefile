ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/spm_v2/

obj-y += mt_cpuidle.o

ifneq ($(CONFIG_ARM64), y)
obj-y += cpu_dormant-v7.o
else
obj-y += cpu_dormant.o
endif

