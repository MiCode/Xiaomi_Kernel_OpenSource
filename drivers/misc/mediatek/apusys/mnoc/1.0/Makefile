ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

ccflags-y += -I$(obj)/../$(MTK_PLATFORM)

obj-y += apusys_mnoc.o

apusys_mnoc-objs:= mnoc_drv.o mnoc_dbg.o mnoc_qos.o mnoc_pmu.o ../$(MTK_PLATFORM)/mnoc_hw.o
