#
# Makefile for Invensense inv-mpu-iio device.
#

obj-$(CONFIG_INV_MPU_IIO) += inv-mpu-iio.o

inv-mpu-iio-objs += inv_mpu_common.o
inv-mpu-iio-objs += inv_mpu_ring.o
inv-mpu-iio-objs += inv_mpu_timestamp.o
inv-mpu-iio-objs += inv_mpu_dts.o

# chip support
ifeq ($(CONFIG_INV_MPU_IIO_ICM20648), y)
inv-mpu-iio-objs += icm20648/inv_mpu_init.o
inv-mpu-iio-objs += icm20648/inv_mpu_core.o
inv-mpu-iio-objs += icm20648/inv_mpu_parsing.o
inv-mpu-iio-objs += icm20648/inv_mpu_setup.o
inv-mpu-iio-objs += icm20648/inv_mpu_dmp_fifo.o
inv-mpu-iio-objs += icm20648/inv_slave_compass.o
inv-mpu-iio-objs += icm20648/inv_slave_pressure.o
inv-mpu-iio-objs += icm20648/inv_slave_als.o
inv-mpu-iio-objs += icm20648/inv_mpu_load_dmp.o
inv-mpu-iio-objs += icm20648/inv_mpu_selftest.o
inv-mpu-iio-objs += dmp_support/inv_mpu_misc.o
else ifeq ($(CONFIG_INV_MPU_IIO_ICM20690), y)
inv-mpu-iio-objs += icm20690/inv_mpu_init_20690.o
inv-mpu-iio-objs += icm20690/inv_mpu_core_20690.o
inv-mpu-iio-objs += icm20690/inv_mpu_parsing_20690.o
inv-mpu-iio-objs += icm20690/inv_mpu_setup_20690.o
inv-mpu-iio-objs += icm20690/inv_mpu_selftest_20690.o
inv-mpu-iio-objs += icm20690/inv_slave_compass.o
else ifeq ($(CONFIG_INV_MPU_IIO_ICM20602), y)
inv-mpu-iio-objs += icm20602/inv_mpu_init_20602.o
inv-mpu-iio-objs += icm20602/inv_mpu_core_20602.o
inv-mpu-iio-objs += icm20602/inv_mpu_parsing_20602.o
inv-mpu-iio-objs += icm20602/inv_mpu_setup_20602.o
inv-mpu-iio-objs += icm20602/inv_mpu_selftest_20602.o
else ifeq ($(CONFIG_INV_MPU_IIO_ICM20608D), y)
inv-mpu-iio-objs += icm20608d/inv_mpu_init_20608.o
inv-mpu-iio-objs += icm20608d/inv_mpu_core_20608.o
inv-mpu-iio-objs += icm20608d/inv_mpu_parsing_20608.o
inv-mpu-iio-objs += icm20608d/inv_mpu_setup_20608D.o
inv-mpu-iio-objs += icm20608d/inv_mpu_dmp_fifo.o
inv-mpu-iio-objs += icm20608d/inv_mpu_load_dmp.o
inv-mpu-iio-objs += icm20608d/inv_mpu_selftest_20608.o
inv-mpu-iio-objs += dmp_support/inv_mpu_misc.o
else ifeq ($(CONFIG_INV_MPU_IIO_IAM20680), y)
inv-mpu-iio-objs += iam20680/inv_mpu_init_20680.o
inv-mpu-iio-objs += iam20680/inv_mpu_core_20680.o
inv-mpu-iio-objs += iam20680/inv_mpu_parsing_20680.o
inv-mpu-iio-objs += iam20680/inv_mpu_setup_20680.o
inv-mpu-iio-objs += iam20680/inv_mpu_selftest_20680.o
endif

# Bus support
obj-$(CONFIG_INV_MPU_IIO_I2C) += inv-mpu-iio-i2c.o
inv-mpu-iio-i2c-objs := inv_mpu_i2c.o
obj-$(CONFIG_INV_MPU_IIO_SPI) += inv-mpu-iio-spi.o
inv-mpu-iio-spi-objs := inv_mpu_spi.o

obj-y += inv_test/
