ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/common
ccflags-y += -Idrivers/media/platform/msm/camera_v2/msm_vb2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/camera
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
obj-$(CONFIG_MSMB_CAMERA) += cci/ io/ csiphy/ csid/ actuator/ eeprom/ ois/ flash/ ir_led/ ir_cut/
obj-$(CONFIG_MSMB_CAMERA) += laser_led/
obj-$(CONFIG_MSM_CAMERA_SENSOR) += msm_sensor_init.o msm_sensor_driver.o msm_sensor.o

ifeq ($(TARGET_PRODUCT), olive)
KBUILD_CFLAGS += -DOLIVE_MSM_CAMERA_HW_INFO
KBUILD_CFLAGS += -DFLASH_ACTIVE_5V_ENABLE
else ifeq ($(TARGET_PRODUCT), olivelite)
KBUILD_CFLAGS += -DOLIVELITE_MSM_CAMERA_HW_INFO
KBUILD_CFLAGS += -DFLASH_ACTIVE_5V_ENABLE
else
endif
