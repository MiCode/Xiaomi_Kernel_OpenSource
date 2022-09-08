ifneq ($CONFIG_MTK_SENSOR_SUPPORT), n)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorHub/inc_v1

obj-$(CONFIG_MTK_SENSOR_SUPPORT) += hwmon.o
hwmon-$(CONFIG_MTK_SENSOR_SUPPORT) := sensor_event/sensor_event.o \
        sensor_attributes/sensor_attr.o \
        hwmsen/hwmsen_helper.o \
        sensor_performance/sensor_performance.o

obj-$(CONFIG_MTK_SENSOR_SUPPORT) += sensor_list/sensor_list.o

else
obj-n := hwmsen/
endif
