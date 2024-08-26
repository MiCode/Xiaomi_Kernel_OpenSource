ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/alsps/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorHub/inc_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp
#N19A code for HQ-359826 by chenyushuai at 2023/12/20 start
ccflags-y += -I$(srctree)/drivers/input/touchscreen/tpd_notify
#N19A code for HQ-359826 by chenyushuai at 2023/12/20 end
obj-$(CONFIG_MTK_SENSOR_SUPPORT)	:=  alspshub.o

