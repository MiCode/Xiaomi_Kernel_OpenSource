
# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/synaptics_i2c_rmi4/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/synaptics_i2c_rmi4/include/$(CONFIG_SYNA_RMI4_FIRMWARE)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-$(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_I2C_RMI4)	+= synaptics_driver.o
obj-$(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_DSX_RMI4_DEV)	+= rmi_dev.o
obj-$(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_DSX_RMI4_FW_UPDATE) += rmi_fw_update.o
