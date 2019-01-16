ifeq ($(CONFIG_MTK_HDMI_SUPPORT),y)
ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI),nxp_tda19989)
ccflags-y += -DTMFL_TDA19989 -DTMFL_NO_RTOS -DTMFL_LINUX_OS_KERNEL_DRIVER

obj-y += 						\
	hdmi_drv.o					\
	tmbslTDA9989_HDCP.o			\
	tmbslTDA9989_local.o		\
	tmbslTDA9989_State.o		\
	tmdlHdmiTx_local.o			\
	tmbslTDA9989_Edid.o			\
	tmbslTDA9989_InOut.o		\
	tmbslTDA9989_Misc.o			\
	tmdlHdmiTx.o				\
	tmdlHdmiTx_cfg.o			\
	I2C_api.o
endif
endif
obj-y += dummy.o
