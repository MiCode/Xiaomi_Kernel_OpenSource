GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)

ifeq ($(CONFIG_MSM_CAMERA_V4L2),y)
  EXTRA_CFLAGS += -Idrivers/media/video/msm
  EXTRA_CFLAGS += -Idrivers/media/video/msm/io
  EXTRA_CFLAGS += -Idrivers/media/video/msm/csi
  EXTRA_CFLAGS += -Idrivers/media/video/msm/eeprom
  EXTRA_CFLAGS += -Idrivers/media/video/msm/sensors
  EXTRA_CFLAGS += -Idrivers/media/video/msm/actuators
  obj-$(CONFIG_MSM_CAMERA)   += msm_cam_server.o
endif
