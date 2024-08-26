# In case the platform does NOT support this type of sensors

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include

obj-$(CONFIG_MTK_SENSOR_SUPPORT)	+= situation.o
obj-y += situation_hub/
# N19a code for HQ-353506 by huweifeng at 2023/12/16 start
obj-$(CONFIG_MTK_SAR_ALGO_HUB)				+=	sar_algo/
obj-$(CONFIG_MTK_SAR_ALGO_HUB)				+=	sar_algo_top/
# N19a code for HQ-353506 by huweifeng at 2023/12/16 end