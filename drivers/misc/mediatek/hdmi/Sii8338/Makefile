
include $(srctree)/drivers/misc/mediatek/Makefile.custom

#AMF_SRC:= yes




ifeq ($(AMF_SRC),yes)
ccflags-y += -I$(src)/linux_driver/platform/amf
ccflags-y += -I$(src)/linux_driver/platform/amf/AMF_LIB_Src
ccflags-y += -I$(src)/linux_driver/platform/amf/AMF_LIB_Src/HAL
ccflags-y += -I$(src)/linux_driver/platform/amf/AMF_LIB_Src/DEBUG
ccflags-y += -I$(src)/linux_driver/platform/amf/AMF_LIB_Src/CEC
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/inc

ccflags-y += -DMAKE_8338_DRIVER

ccflags-y += -DRGB_BOARD #

ifeq ($(AMF_SRC),yes)
ccflags-y += -DAMF_SRC
endif

ccflags-$(CONFIG_DEBUG_DRIVER) += -DDEBUG


#----------------- App layer --------------------------------------
obj-y += mhl_linuxdrv_main.o
obj-y += mhl_linuxdrv_ioctl.o
obj-y += mhl_linuxdrv_open_close.o

#----------------- Component layer --------------------------------
obj-y += si_mhl_tx.o
obj-y += si_mhl_tx_rcp_table.o

#----------------- Platform layer ---------------------------------
obj-y += sii_osal_linux_timer.o
obj-y += si_osdebug.o
obj-y += sii_hal_linux_init.o
obj-y += sii_hal_linux_i2c.o
obj-y += sii_hal_linux_isr.o
obj-y += sii_hal_linux_timer.o
obj-y += sii_hal_linux_gpio.o

#----------------- Driver layer --------------------------------
obj-y += si_cra.o
obj-y += si_cra_cfg.o

#----------------- MHL layer --------------------------------
obj-y += si_drv_mhl_tx_edid.o
#obj-y += si_drv_mhl_rgb.o
#obj-y += si_drv_ext.o
obj-y += si_drv_mhl_tx.o
obj-y += smartbook.o
obj-y += hdmi_drv.o

#----------------- MDT          --------------------------------
obj-y += si_drv_mdt_tx.o
obj-y += si_mdt_inputdev.o

ifeq ($(AMF_SRC),yes)	
#----------------- AMF RGB  board Control layer ---------------------------------
obj-y += linux_driver/platform/amf/AMF_LIB_Src/amf_timers.o
obj-y += linux_driver/platform/amf/AMF_LIB_Src/amf_regio.o
obj-y += linux_driver/platform/amf/AMF_LIB_Src/amf_interrupt.o
obj-y += linux_driver/platform/amf/AMF_LIB_Src/amf_EDID.o
obj-y += linux_driver/platform/amf/AMF_LIB_Src/HAL/amf_hal_cp9233.o
obj-y += linux_driver/platform/amf/AMF_LIB_Src/amf_main.o
endif
