subdir-ccflags-y += -I$(srctree)/kernel/sched
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

obj-$(CONFIG_MTK_SCHED_EXTENSION) += eas_plus.o debug.o tuning.o debug_aee.o

ifdef CONFIG_MTK_AEE_IPANIC
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
endif

