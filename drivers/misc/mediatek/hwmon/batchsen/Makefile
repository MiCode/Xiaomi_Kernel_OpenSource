ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include
ifeq ($(CONFIG_MTK_SCP_SENSORHUB_V1),y)
obj-y := batch_v1.o
else
obj-y := batch.o
endif
