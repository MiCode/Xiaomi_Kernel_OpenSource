
# Kernel makefile for motions sensors
#
#

EXTRA_CFLAGS += -Idrivers/misc/inv_mpu
EXTRA_CFLAGS += -D__C99_DESIGNATED_INITIALIZER
EXTRA_CFLAGS += -DINV_CACHE_DMP=1

obj-$(CONFIG_MPU_SENSORS_TIMERIRQ)+= timerirq.o
obj-$(CONFIG_INV_SENSORS) += mpuirq.o slaveirq.o

obj-$(CONFIG_MPU_SENSORS_MPU3050) += mpu3050/
obj-$(CONFIG_MPU_SENSORS_MPU6050B1) += mpu6050/

obj-y += accel/
obj-y += compass/
obj-y += pressure/
