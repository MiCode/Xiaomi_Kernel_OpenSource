ifeq ($(CONFIG_MTK_COMBO), y)
ccflags-y += -I$(src)/../../linux/include \
						 -I$(src)/../../linux/pri/include \
						 -I$(src)/../../core/include \
						 -I$(src)/../../include \
						 -I$(src)/../ \
						 -I$(srctree)/drivers/misc/mediatek/btif/inc \
						 -I$(srctree)/drivers/misc/mediatek/connectivity/common_detect \
                         -I$(src)/../include

ifeq ($(CONFIG_ARM64), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/mt6752/include/mach
endif

ccflags-y += -DWMT_CREATE_NODE_DYNAMIC=1
obj-y += stp_btif.o \
         stp_dbg.o \
         stp_exp.o \
         wmt_dev.o \
         wmt_exp.o

endif
