
# Kernel makefile for motions sensors
#
#

obj-$(CONFIG_MPU_SENSORS_MPU3050) += mpu3050.o

ccflags-y := -DMPU_CURRENT_BUILD_MPU3050

mpu3050-objs += mldl_cfg.o
mpu3050-objs += mldl_print_cfg.o
mpu3050-objs += mlsl-kernel.o
mpu3050-objs += mpu-dev.o

EXTRA_CFLAGS += -Idrivers/misc/inv_mpu
EXTRA_CFLAGS += -D__C99_DESIGNATED_INITIALIZER
EXTRA_CFLAGS += -DINV_CACHE_DMP=1
