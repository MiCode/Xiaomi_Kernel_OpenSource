GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)

ifeq ($(CONFIG_MSM_CAMERA_V4L2),y)
  EXTRA_CFLAGS += -Idrivers/media/platform/msm/camera_v1
  EXTRA_CFLAGS += -Idrivers/media/platform/msm/camera_v1/io
  EXTRA_CFLAGS += -Idrivers/media/platform/msm/camera_v1/csi
  EXTRA_CFLAGS += -Idrivers/media/platform/msm/camera_v1/eeprom
  EXTRA_CFLAGS += -Idrivers/media/platform/msm/camera_v1/sensors
  EXTRA_CFLAGS += -Idrivers/media/platform/msm/camera_v1/actuators
  obj-$(CONFIG_MSM_CAMERA)   += msm_cam_server.o
endif
