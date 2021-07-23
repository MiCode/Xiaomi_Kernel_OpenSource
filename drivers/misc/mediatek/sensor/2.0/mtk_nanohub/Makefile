ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/core
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/mtk_nanohub/nanohub
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(CONFIG_MTK_PLATFORM)
ccflags-$(CONFIG_MACH_MT6893) += -I$(srctree)/drivers/misc/mediatek/scp/mt6885
$(warning "config_mtk_platform:$(CONFIG_MTK_PLATFORM)")
obj-y += nanohub/
obj-y += mtk_nanohub.o mtk_nanohub_ipi.o sensor_list.o

ifeq ($(CONFIG_J22_SAR),y)
ccflags-y += -D CONFIG_J22_SAR
endif