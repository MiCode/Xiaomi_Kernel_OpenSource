ifneq ($(CONFIG_MTK_SENSOR_SUPPORT), n)
obj-$(CONFIG_CUSTOM_KERNEL_SENSORHUB) += sensorHub/
obj-$(CONFIG_MTK_HWMON) += hwmon/
obj-y += sensor_probe/
obj-$(CONFIG_CUSTOM_KERNEL_ALSPS) += alsps/
ifeq ($(CONFIG_CUSTOM_KERNEL_ACCELGYRO),y)
obj-y += accelgyro/
else
obj-$(CONFIG_CUSTOM_KERNEL_ACCELEROMETER) += accelerometer/
obj-$(CONFIG_CUSTOM_KERNEL_GYROSCOPE) += gyroscope/
endif
obj-$(CONFIG_CUSTOM_KERNEL_MAGNETOMETER) += magnetometer/
obj-$(CONFIG_CUSTOM_KERNEL_BAROMETER) += barometer/
obj-$(CONFIG_CUSTOM_KERNEL_STEP_COUNTER) += step_counter/
obj-$(CONFIG_CUSTOM_KERNEL_SIGNIFICANT_MOTION_SENSOR) += step_counter/
obj-$(CONFIG_CUSTOM_KERNEL_SITUATION) += situation/
obj-$(CONFIG_CUSTOM_KERNEL_SENSOR_FUSION) += sensorfusion/
else
obj-y += dummy.o
endif
