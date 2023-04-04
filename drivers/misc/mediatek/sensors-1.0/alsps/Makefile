# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
obj-$(CONFIG_MTK_SENSOR_SUPPORT)   +=  alsps_common.o
alsps_common-$(CONFIG_MTK_SENSOR_SUPPORT) := alsps.o alsps_factory.o aal_control.o

obj-$(CONFIG_MTK_ALSPSHUB) += alspshub/
ifeq ($(CONFIG_MTK_CM36558),y)
obj-$(CONFIG_MTK_CM36558) += cm36558/
endif
ifeq ($(CONFIG_MTK_LTR303),y)
obj-$(CONFIG_MTK_LTR303) += ltr303/
endif
ifeq ($(CONFIG_MTK_LTR559),y)
obj-$(CONFIG_MTK_LTR559)	+=  ltr559/
endif
ifeq ($(CONFIG_MTK_STK3X1X_NEW),y)
obj-$(CONFIG_MTK_STK3X1X_NEW)   +=  stk3x1x/
endif
