#
# Makefile for industrial I/O Magnetometer sensor drivers
#

# When adding new entries keep the list in alphabetical order
obj-$(CONFIG_AK8975)	+= ak8975.o
obj-$(CONFIG_AK09911)	+= ak09911.o
obj-$(CONFIG_MAG3110)	+= mag3110.o
obj-$(CONFIG_HID_SENSOR_MAGNETOMETER_3D) += hid-sensor-magn-3d.o

obj-$(CONFIG_IIO_ST_MAGN_3AXIS) += st_magn.o
st_magn-y := st_magn_core.o
st_magn-$(CONFIG_IIO_BUFFER) += st_magn_buffer.o

obj-$(CONFIG_IIO_ST_MAGN_I2C_3AXIS) += st_magn_i2c.o
obj-$(CONFIG_IIO_ST_MAGN_SPI_3AXIS) += st_magn_spi.o

bmm050-y := bmm050_driver.o bmm050_api.o bmm050_buffer.o bmm050_trigger.o
obj-$(CONFIG_SENSORS_BMM050)    += bmm050.o
ccflags-$(CONFIG_SENSORS_BMM050)    += -DBMM_USE_BASIC_I2C_FUNC
