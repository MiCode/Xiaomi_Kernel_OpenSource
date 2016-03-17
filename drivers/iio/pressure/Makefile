#
# Makefile for industrial I/O pressure drivers
#

obj-$(CONFIG_HID_SENSOR_PRESSURE) += hid-sensor-pressure.o
obj-$(CONFIG_IIO_ST_PRESS) += st_pressure.o
st_pressure-y := st_pressure_core.o
st_pressure-$(CONFIG_IIO_BUFFER) += st_pressure_buffer.o

obj-$(CONFIG_IIO_ST_PRESS_I2C) += st_pressure_i2c.o
obj-$(CONFIG_IIO_ST_PRESS_SPI) += st_pressure_spi.o

obj-$(CONFIG_BMP280)     += bmp280_core.o BMP280.o
obj-$(CONFIG_BMP280_I2C) += bmp280_i2c.o
obj-$(CONFIG_BMP280_SPI) += bmp280_spi.o
