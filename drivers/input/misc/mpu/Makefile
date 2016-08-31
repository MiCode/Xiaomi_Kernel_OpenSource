#
# Makefile for Invensense inv-mpu device.
#

obj-$(CONFIG_INV_MPU) += inv-mpu.o

inv-mpu-objs := inv_gyro.o
inv-mpu-objs += inv_gyro_misc.o
inv-mpu-objs += inv_mpu3050.o
inv-mpu-objs += dmpDefaultMPU6050.o

# the Kionix KXTF9 driver is added to the inv-mpu device driver because it
# must be connected to an MPU3050 device on the secondary slave bus.
inv-mpu-objs += inv_slave_kxtf9.o

