# Makefile for Altek mini isp driver

ccflags-y += -DNEW_ABS

# add obj to linux kernel
altek_isp-objs := \
	basic_setting_cmd.o bulk_data_cmd.o \
	camera_profile_cmd.o miniisp_intf_i2c.o \
	miniisp_intf_spi.o miniisp_isp.o \
	ispctrl_if_master.o miniisp_customer_define.o \
	miniisp_top.o miniisp_ctrl.o \
	operation_cmd.o sys_manage_cmd.o \
	miniisp_utility.o miniisp_debug_if.o \
	miniisp_chip_base_define.o miniisp_ctrl_intf.o

obj-$(CONFIG_VIDEO_ALTEK_ISP)		+= altek_isp.o
