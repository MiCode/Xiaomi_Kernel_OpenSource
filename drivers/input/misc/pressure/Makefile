#
# Makefile for stand-alone pressure input device drivers
#

obj-$(CONFIG_INV_BMP180) += inv-bmp180.o
inv-bmp180-objs += bmp180.o

obj-$(CONFIG_BMP280)     += bmp280_core.o BMP280.o
obj-$(CONFIG_BMP280_I2C) += bmp280_i2c.o
obj-$(CONFIG_BMP280_SPI) += bmp280_spi.o
