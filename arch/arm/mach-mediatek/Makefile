subdir-ccflags-y += -Werror

ifeq ($(CONFIG_SMP),y)
obj-$(CONFIG_ARCH_MEDIATEK) += platsmp.o
endif

ifdef CONFIG_ARCH_MT6580
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
endif

ifdef CONFIG_ARCH_MT6570
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
endif


obj-$(CONFIG_ARCH_MEDIATEK) += mediatek.o
obj-$(CONFIG_MEDIATEK_SOLUTION)	+= core.o

