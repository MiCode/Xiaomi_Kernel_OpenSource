
# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/synaptics_i2c_rmi4/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/synaptics_i2c_rmi4/include/$(CONFIG_SYNA_RMI4_FIRMWARE)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-$(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_I2C_RMI4)	+= synaptics_driver.o
obj-$(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_DSX_RMI4_DEV)	+= rmi_dev.o
obj-$(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_DSX_RMI4_FW_UPDATE) += rmi_fw_update.o

obj-y   +=  tpd_control.o
obj-y   +=  tpd_button.o
obj-y   +=  tpd_debug.o
obj-y   +=  tpd_default.o
obj-y   +=  tpd_misc.o
obj-y   +=  tpd_setting.o
