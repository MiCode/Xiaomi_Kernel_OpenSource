# In case the platform does NOT support this type of sensors

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include

obj-y	+= situation.o
obj-$(CONFIG_MTK_INPKHUB)	+=	inpocket/
obj-$(CONFIG_MTK_STATHUB) 	+= 	stationary/
obj-$(CONFIG_MTK_WAKEHUB)	+=	wake_gesture/
obj-$(CONFIG_MTK_GLGHUB)	+=	glance_gesture/
obj-$(CONFIG_MTK_PICKUPHUB)	+=	pickup_gesture/
obj-$(CONFIG_MTK_ANSWER_CALL_HUB)		+=	answercall/
obj-$(CONFIG_MTK_DEVICE_ORIENTATION_HUB)	+=	device_orientation/
obj-$(CONFIG_MTK_MOTION_DETECT_HUB)			+=	motion_detect/
obj-$(CONFIG_MTK_TILTDETECTHUB)				+=	tilt_detector/
obj-$(CONFIG_MTK_FLAT_HUB)				+=	flat/
obj-$(CONFIG_MTK_SAR_HUB)				+=	sar/
obj-$(CONFIG_PS_FACTORY_STRM)				+=	ps_factory_stream/
obj-$(CONFIG_ALS_FACTORY_STRM)				+=	als_factory_stream/
obj-$(CONFIG_SAR_ALGO)				+=	sar_algo/
obj-$(CONFIG_ELEVATOR_DETECT)				+=	elevator_detect/