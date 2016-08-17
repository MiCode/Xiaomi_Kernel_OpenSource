#
# Makefile for Synaptics Touchscreen (RMI4/SPI)
#
GCOV_PROFILE := y

ccflags-$(CONFIG_SPI_DEBUG) := -DDEBUG

#Synaptics SPI Sensor (2002)
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_bus.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_i2c.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_spi.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_driver.o rmi_f01.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_f09.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_f11.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_f19.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_f34.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_f54.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_dev.o
