include $(srctree)/drivers/misc/mediatek/Makefile.custom
#ccflags-y += -I$(src)/core/include
subdir-ccflags-y += -DWMT_PLAT_APEX=0
subdir-ccflags-y += -DWMT_PLAT_ALPS=1 
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/				
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/eemcs/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/conn_md/include
#subdir-ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/dct/dct
subdir-ccflags-y += -I$(DRVGEN_OUT)/inc
subdir-ccflags-y += -DWMT_UART_RX_MODE_WORK=0 # 1. work thread 0. tasklet
subdir-ccflags-y += -DREMOVE_MK_NODE=1
subdir-ccflags-y += -DWMT_SDIO_MODE=1
ccflags-y += -I$(src)/linux/include \
				-I$(src)/core/include \
				-I$(src)/include \
				-I$(src)/ \
				-I$(srctree)/drivers/misc/mediatek/connectivity/common_detect

						 
#ifeq ($(CONFIG_MTK_COMBO_CHIP), MT6628)
#EXT_FLAG=
#else
#EXT_FLAG=_mt6628
#endif
EXT_FLAG=
COMMON_SRC_PATH := $(src)
COMMON_OBJ_PATH := $(src)


ifeq ($(CONFIG_MTK_COMBO),m)

# WMT DRIVER
obj-$(CONFIG_MTK_COMBO_COMM) += mtk_stp_wmt$(EXT_FLAG).o
# WMT DRIVER-core part
mtk_stp_wmt$(EXT_FLAG)-objs	:= core/wmt_core.o core/wmt_ctrl.o  core/wmt_func.o  core/wmt_lib.o core/wmt_conf.o core/wmt_dbg.o core/wmt_exp.o

mtk_stp_wmt$(EXT_FLAG)-objs	+= core/wmt_ic_6620.o
mtk_stp_wmt$(EXT_FLAG)-objs	+= core/wmt_ic_6628.o
mtk_stp_wmt$(EXT_FLAG)-objs	+= core/wmt_ic_6630.o

# WMT DRIVER-linux private part
mtk_stp_wmt$(EXT_FLAG)-objs	+= linux/wmt_dev.o

# WMT DRIVER-OSAL
mtk_stp_wmt$(EXT_FLAG)-objs	+= linux/osal.o
# WMT DRIVER-platform implementation
#ccflags-y += -D WMT_PLAT_ALPS #Jake modify

# WMT DRIVER-platform part, specified by string CONFIG_MTK_COMBO_PLAT_PATH
PLAT := $(shell echo $(CONFIG_MTK_COMBO_PLAT_PATH))


#obj-y += platform/$(PLAT)/mtk_wcn_cmb_stub_$(PLAT).o
mtk_stp_wmt$(EXT_FLAG)-objs	+= platform/$(PLAT)/wmt_plat_$(PLAT).o
mtk_stp_wmt$(EXT_FLAG)-objs	+= platform/$(PLAT)/wmt_plat_stub.o

# FIXME: select desired hw according to project configuration
#ifeq ($(CONFIG_MTK_COMBO_CHIP), MT6628)
#mtk_stp_wmt$(EXT_FLAG)-objs	+= platform/alps/mtk_wcn_cmb_hw_6628.o 
#else
mtk_stp_wmt$(EXT_FLAG)-objs	+= platform/$(PLAT)/mtk_wcn_cmb_hw.o
#endif

mtk_stp_wmt$(EXT_FLAG)-objs	+= core/stp_exp.o core/stp_core.o core/psm_core.o core/btm_core.o linux/stp_dbg.o
#ifeq ($(CONFIG_MTK_COMBO_CHIP), MT6628)
# WMT stub part (built-in kernel image)
#obj-y			+= platform/$(PLAT)/mtk_wcn_cmb_stub_$(PLAT).o
#endif

obj-$(CONFIG_MTK_COMBO_COMM_UART) += mtk_stp_uart$(EXT_FLAG).o
mtk_stp_uart$(EXT_FLAG)-objs	:= linux/stp_uart.o


obj-$(CONFIG_MTK_COMBO_COMM) += mtk_hif_sdio$(EXT_FLAG).o
mtk_hif_sdio$(EXT_FLAG)-objs	:= linux/hif_sdio.o
#mtk_hif_sdio$(EXT_FLAG)-objs	+= linux/hif_sdio_chrdev.o
#mtk_hif_sdio$(EXT_FLAG)-objs	+= platform/$(PLAT)/mtk_wcn_cmb_hw.o
#mtk_hif_sdio$(EXT_FLAG)-objs	+= platform/$(PLAT)/wmt_plat_$(PLAT).o
mtk_hif_sdio$(EXT_FLAG)-objs	+= linux/osal.o


obj-$(CONFIG_MTK_COMBO_COMM_SDIO) += mtk_stp_sdio$(EXT_FLAG).o
mtk_stp_sdio$(EXT_FLAG)-objs	:= linux/stp_sdio.o
mtk_stp_sdio$(EXT_FLAG)-objs	+= linux/osal.o


obj-$(CONFIG_MTK_COMBO_BT) += mtk_stp_bt$(EXT_FLAG).o
mtk_stp_bt$(EXT_FLAG)-objs		:= linux/stp_chrdev_bt.o

obj-$(CONFIG_MTK_COMBO_ANT) += mtk_stp_ant$(EXT_FLAG).o
mtk_stp_ant$(EXT_FLAG)-objs		:= linux/stp_chrdev_ant.o

obj-$(CONFIG_MTK_COMBO_GPS) += mtk_stp_gps$(EXT_FLAG).o
mtk_stp_gps$(EXT_FLAG)-objs	:= linux/stp_chrdev_gps.o


obj-$(CONFIG_MTK_COMBO_WIFI) += mtk_wmt_wifi$(EXT_FLAG).o
mtk_wmt_wifi$(EXT_FLAG)-objs       := linux/wmt_chrdev_wifi.o 
endif

ifeq ($(CONFIG_MTK_COMBO),y)
obj-y += core/
obj-y += linux/
obj-y += platform/

endif
