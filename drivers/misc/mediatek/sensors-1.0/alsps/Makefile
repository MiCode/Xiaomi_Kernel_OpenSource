# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorHub/inc_v1
obj-y   +=  alsps.o
obj-y   +=  alsps_factory.o
obj-y   +=  aal_control.o
#add psensor vdd3 compile by luozeng at 2021.3.24 start#
obj-y   +=  ldo_alsps.o
#add psensor vdd3 compile by luozeng at 2021.3.24 end#
obj-$(CONFIG_MTK_ALSPSHUB)      +=  alspshub/
obj-$(CONFIG_MTK_CM36558)	+=  cm36558/
obj-$(CONFIG_MTK_LTR303)	+=  ltr303/
obj-$(CONFIG_MTK_LTR559)	+=  ltr559/
