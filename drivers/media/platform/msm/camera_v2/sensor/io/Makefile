ifeq ($(TARGET_PRODUCT),scorpio)
	ifeq ($(CONFIG_CAMERA_ALS_ENABLE),y)
		EXTRA_CFLAGS += -DCAMERA_ALS_ENABLE
	endif
endif
ccflags-y += -Idrivers/media/platform/msm/camera_v2/
ccflags-y += -Idrivers/media/platform/msm/camera_v2/common
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
obj-$(CONFIG_MSMB_CAMERA)   += msm_camera_cci_i2c.o msm_camera_qup_i2c.o msm_camera_spi.o msm_camera_dt_util.o
