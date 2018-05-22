#
# Makefile for the vl53L0X drivers.
#

# Each configuration option enables a list of files.
FEATURE_USE_CCI := false
#FEATURE_USE_CCI := true

ifeq ($(FEATURE_USE_CCI), true)
ccflags-y	+= -Idrivers/input/misc/vl53l0x/inc -DCAMERA_CCI
else
ccflags-y	+= -Idrivers/input/misc/vl53l0x/inc -DSTM_TEST
endif

ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/common
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
obj-$(CONFIG_STMVL53L0X)			+= stmvl53l0x.o
stmvl53l0x-objs				:= stmvl53l0x_module.o stmvl53l0x_module-i2c.o stmvl53l0x_module-cci.o src/vl53l0x_api_calibration.o src/vl53l0x_api_core.o src/vl53l0x_api_ranging.o src/vl53l0x_api_strings.o src/vl53l0x_api.o src/vl53l0x_platform.o src/vl53l0x_i2c_platform.o src/vl53l0x_port_i2c.o
