ifeq ($(CONFIG_MTK_COMBO), y)
ccflags-y += -I$(src)/../../linux/include \
						 -I$(src)/../../linux/pri/include \
						 -I$(src)/../../core/include \
						 -I$(src)/../../include \
						 -I$(src)/../ \
						 -I$(srctree)/drivers/misc/mediatek/connectivity/common_detect \
                         -I$(src)/../include

ccflags-y += -DWMT_CREATE_NODE_DYNAMIC=1

ifeq ($(CONFIG_MTK_TC1_FEATURE), y)
    ccflags-y += -DCFG_TC1_FEATURE=1
else
    ccflags-y += -DCFG_TC1_FEATURE=0
endif


obj-y += osal.o \
         bgw_desense.o \
         stp_chrdev_bt.o \
         stp_chrdev_gps.o \
         wmt_chrdev_wifi.o \
         wmt_idc.o

endif
