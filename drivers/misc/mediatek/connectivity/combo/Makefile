# combo driver: MT6620
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	#subdir-ccflags-y can be used in 2.6.34 in the future
	#subdir-ccflags-y += -I$(src)/common/include -I$(src)/common/linux/include 
    obj-y += common/
    obj-$(CONFIG_MTK_COMBO_WIFI) += drv_wlan/
    obj-n := dummy.o
	#skip bt hci driver
	#obj-y	+= drv_bt/

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
