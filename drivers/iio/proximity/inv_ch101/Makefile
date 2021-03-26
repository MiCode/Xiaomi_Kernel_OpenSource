# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_CH101)	+= ch101-core.o
ch101-core-objs		+= ch101_core.o
ch101-core-objs		+= ch101_sysfs.o
ch101-core-objs 	+= src/init_driver.o
ch101-core-objs 	+= src/chbsp_dummy.o
ch101-core-objs 	+= src/chbsp_init.o
ch101-core-objs 	+= src/chirp_hal.o
ch101-core-objs 	+= src/i2c_hal.o
ch101-core-objs 	+= src/ch101_gpr.o
ch101-core-objs 	+= src/ch101_gpr_fw.o
ch101-core-objs 	+= src/ch101_gpr_open.o
ch101-core-objs 	+= src/ch101_gpr_open_fw.o
ch101-core-objs 	+= src/ch101_gpr_sr_open.o
ch101-core-objs 	+= src/ch101_gpr_sr_open_fw.o
ch101-core-objs 	+= src/ch_api.o
ch101-core-objs 	+= src/ch_common.o
ch101-core-objs 	+= src/ch_driver.o
ch101-core-objs         += src/ch201_gprmt_fw.o

obj-$(CONFIG_CH101_I2C)	+= ch101-i2c.o
ch101-i2c-objs		+= ch101_i2c.o

# disable some warning flags
ccflags-y += -Wno-format
