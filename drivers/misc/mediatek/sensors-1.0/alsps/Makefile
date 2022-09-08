# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
obj-$(CONFIG_MTK_SENSOR_SUPPORT)   +=  alsps_common.o
alsps_common-$(CONFIG_MTK_SENSOR_SUPPORT) := alsps.o alsps_factory.o aal_control.o

obj-$(CONFIG_MTK_ALSPSHUB)      +=  alspshub/
#obj-$(CONFIG_MTK_CM36558)	+=  cm36558/
#obj-$(CONFIG_MTK_LTR303)	+=  ltr303/
