# In case the platform does NOT support this type of sensors

ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include

obj-y	+= gesture.o

ifeq ($(CONFIG_MTK_SCP_SENSORHUB_V1),y)
obj-$(CONFIG_MTK_INPKHUB)	+=	inpocket/
obj-$(CONFIG_MTK_STATHUB) 	+= 	stationary/
endif

ifeq ($(CONFIG_CUSTOM_KERNEL_IN_CALL_SENSOR),"OTHER_VENDOR")
obj-y += other_vendor/
endif

