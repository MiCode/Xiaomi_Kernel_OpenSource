#
# Makefile for the STMicroelectronics sensor-hub core driver.
#

obj-$(CONFIG_IIO_ST_HUB_I2C) += st_sensor_hub_i2c.o
obj-$(CONFIG_IIO_ST_HUB_SPI) += st_sensor_hub_spi.o

obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_accel.o
obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_magn.o
obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_magn_uncalib.o
obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_gyro.o
obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_gyro_uncalib.o
obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_sensor_events.o
obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_quaternion.o
obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_step_detector.o
obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_step_counter.o
obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_euler.o
obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_linear.o
obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_gravity.o
obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_hub_activity.o
obj-$(CONFIG_IIO_ST_HUB_RAM_LOADER) += st_hub_ymodem.o

obj-$(CONFIG_IIO_ST_HUB_DRIVERS) += st_sensor_hub_core.o
st_sensor_hub_core-y := st_sensor_hub.o
