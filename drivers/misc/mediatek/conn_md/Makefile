# CONN-MD Bridge Driver
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
    
    include $(srctree)/drivers/misc/mediatek/Makefile.custom
    ccflags-y += -I$(MTK_CUSTOM_PATH)/eemcs/ -I$(MTK_CUSTOM_PATH)/eccci/
    ccflags-y += -I$(src)/include/
    
    obj-y	+= conn_md_bridge.o
    conn_md_bridge-objs := conn_md.o
    conn_md_bridge-objs += conn_md_exp.o
    conn_md_bridge-objs += conn_md_log.o  
    conn_md_bridge-objs += conn_md_dump.o
    conn_md_bridge-objs += conn_md_dbg.o
    conn_md_bridge-objs += conn_md_test.o
    
# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)
    
default:
	$(MAKE) -C

endif


