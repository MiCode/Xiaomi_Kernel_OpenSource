#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensorHub/inc
ccflags-$(CONFIG_CUSTOM_KERNEL_ACCELEROMETER) += -I$(srctree)/drivers/misc/mediatek/accelerometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/magnetometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/activity_sensor
ccflags-y += -I$(srctree)/drivers/misc/mediatek/face_down_sensor
ccflags-y += -I$(srctree)/drivers/misc/mediatek/glance_gesture_sensor
ccflags-y += -I$(srctree)/drivers/misc/mediatek/heart_rate_sensor
ccflags-y += -I$(srctree)/drivers/misc/mediatek/in_pocket_sensor
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pedometer
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pick_up_sensor
ccflags-y += -I$(srctree)/drivers/misc/mediatek/shake_sensor
ccflags-y += -I$(srctree)/drivers/misc/mediatek/step_counter
ccflags-y += -I$(srctree)/drivers/misc/mediatek/tilt_detector_sensor
ccflags-y += -I$(srctree)/drivers/misc/mediatek/wake_gesture_sensor

obj-y	:=  SCP_sensorHub.o

