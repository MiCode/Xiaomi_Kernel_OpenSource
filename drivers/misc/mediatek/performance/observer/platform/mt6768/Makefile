
MTK_TOP = $(srctree)/drivers/misc/mediatek/

ccflags-y += -I$(MTK_TOP)/performance/observer

#ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/mt6768/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/devfreq
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/

obj-y += qos_pfm.o
obj-y += obpfm_qos_bound.o
