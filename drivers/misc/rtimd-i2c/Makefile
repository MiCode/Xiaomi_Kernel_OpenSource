##########################################
# Makefile for RTIMD-I2C Device Drivers.
##########################################
ccflags-y += -Idrivers/misc/rtimd-i2c
#ccflags-y += -v

obj-$(CONFIG_RTIMD_I2C) += rtimd-i2c.o

ccflags-$(CONFIG_RTIMD_I2C_DEBUG) += -DDEBUG
