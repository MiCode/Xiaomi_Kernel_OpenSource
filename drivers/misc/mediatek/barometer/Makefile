include $(srctree)/drivers/misc/mediatek/Makefile.custom

# In case the platform does NOT support this type of sensors
obj-y   +=  barometer.o barometer_factory.o
ifeq ($(CONFIG_MTK_BMP180_NEW),y)
obj-y += BMP180-new/
endif
