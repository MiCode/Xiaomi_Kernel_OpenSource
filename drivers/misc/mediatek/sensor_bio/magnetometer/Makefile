# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor_bio/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor_bio/accelerometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor_bio/magnetometer/inc
obj-y   +=  mag.o
obj-y   +=  mag_factory.o

obj-$(CONFIG_MTK_MAGHUB)        +=  maghub/
