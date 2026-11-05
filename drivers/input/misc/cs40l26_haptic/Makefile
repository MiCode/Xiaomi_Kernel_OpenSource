# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the input misc drivers.
#

# Each configuration option enables a list of files.


cs40l26-core-objs := cs40l26.o cs40l26-tables.o cs40l26-sysfs.o cs40l26-debugfs.o
obj-$(CONFIG_INPUT_CS40L26)		+= cs40l26-core.o
obj-$(CONFIG_INPUT_CS40L26_I2C)		+= cs40l26-i2c.o
obj-$(CONFIG_INPUT_CS40L26_SPI)		+= cs40l26-spi.o

