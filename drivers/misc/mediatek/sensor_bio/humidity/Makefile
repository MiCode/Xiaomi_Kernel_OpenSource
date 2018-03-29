# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor_bio/hwmon/include

obj-y   +=  humidity.o humidity_factory.o

obj-$(CONFIG_MTK_HMDYHUB)      +=  hmdyhub/
