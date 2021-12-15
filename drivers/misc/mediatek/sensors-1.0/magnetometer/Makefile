# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/accelerometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/magnetometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/include
obj-y   +=  mag.o
obj-y   +=  mag_factory.o

obj-$(CONFIG_MTK_MAGHUB) += maghub/
#obj-$(CONFIG_MTK_AKM09915_NEW) += akm09915/
obj-$(CONFIG_MTK_AKM09918) += akm09918/
