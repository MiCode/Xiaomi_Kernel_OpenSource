#ccflags-y += -I$(srctree)/drivers/staging/android/ion
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/mt6779
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include


ccflags-y += -I$(obj)/../$(MTK_PLATFORM)

obj-m += reviser.o

reviser-objs:= reviser_drv.o reviser_hw.o ../$(MTK_PLATFORM)/reviser_reg.o
