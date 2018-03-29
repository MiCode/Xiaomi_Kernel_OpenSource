# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include

obj-y   +=  humidity.o humidity_factory.o

ifeq ($(CONFIG_MTK_SCP_SENSORHUB_V1),y)
obj-$(CONFIG_MTK_HMDYHUB)      +=  hmdyhub/
else
obj-$(CONFIG_MTK_HTS221) += hts221/
endif
