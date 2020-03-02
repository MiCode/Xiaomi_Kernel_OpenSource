SCP_VERSION := common
SCP_VERSION_PATH := ../$(SCP_VERSION)

obj-y += $(SCP_VERSION_PATH)/
obj-y += adsp_feature_table.o
obj-y += adsp_awake.o
obj-y += adsp_dvfs.o
obj-y += adsp_plat.o
obj-y += adsp_clk.o
obj-y += adsp_bus_monitor.o
obj-y += adsp_reserved_mem.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(CONFIG_MTK_PLATFORM)
