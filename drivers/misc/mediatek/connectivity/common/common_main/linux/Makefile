ifeq ($(CONFIG_MTK_COMBO), y)

ccflags-y += \
	-I$(src)/../linux/include \
	-I$(src)/../core/include \
	-I$(src)/../include \
	-I$(src)/../../common_detect \
	-I$(srctree)/drivers/misc/mediatek/btif/common/inc \
	-I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/mach

ccflags-y += -DWMT_CREATE_NODE_DYNAMIC=1

#ifeq ($(CONFIG_MTK_TC1_FEATURE), y)
#    ccflags-y += -DCFG_TC1_FEATURE=1
#else
#    ccflags-y += -DCFG_TC1_FEATURE=0
#endif

obj-y += osal.o \
	 stp_uart.o \
	 hif_sdio.o \
	 stp_sdio.o \
	 stp_dbg.o \
	 wmt_dbg.o \
         wmt_dev.o \
         wmt_idc.o \
         bgw_desense.o \
	 stp_dbg_combo.o \
	 stp_dbg_soc.o
ifeq ($(CONFIG_MTK_BTIF), y)
obj-y += stp_btif.o
endif

obj-$(CONFIG_MTK_COMBO_BT) += stp_chrdev_bt.o
#obj-$(CONFIG_MTK_COMBO_WIFI) += wmt_chrdev_wifi.o

endif
