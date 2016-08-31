
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_core.o
rmi_core-y := rmi_bus.o rmi_driver.o rmi_f01.o
#rmi_core-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_fw_update.o

# Function drivers
#obj-$(CONFIG_RMI4_F09) += rmi_f09.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_f1a.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_f11.o
#obj-$(CONFIG_RMI4_F17) += rmi_f17.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_f19.o
#obj-$(CONFIG_RMI4_F21) += rmi_f21.o
#obj-$(CONFIG_RMI4_F30) += rmi_f30.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_f31.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_f34.o
#obj-$(CONFIG_RMI4_F41) += rmi_f41.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_f54.o

# Transport layers
obj-$(CONFIG_RMI4_I2C) += rmi_i2c.o
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_spi.o
obj-$(CONFIG_RMI4_SMB) += rmi_smbus.o

# Other features
obj-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += rmi_dev.o


ccflags-$(CONFIG_TOUCHSCREEN_SYN_RMI4_SPI) += -DDEBUG
