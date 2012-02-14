GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)
EXTRA_CFLAGS += -Idrivers/media/video/msm
obj-$(CONFIG_MSM_CAMERA) += msm_camera_i2c.o msm_camera_eeprom.o msm_camera_i2c_mux.o
obj-$(CONFIG_MSM_CAMERA) += msm_io_util.o
