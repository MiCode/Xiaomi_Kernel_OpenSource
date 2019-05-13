ifeq ($(KERNELRELEASE),)

KDIR ?= /home/goodix/workspace/KERNEL_OBJ

PWD := $(shell pwd)

ARCH ?= arm64

CROSS_COMPILE ?= ~/vb/alps/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-

modules:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions Module.symvers modules.order

else

obj-$(CONFIG_FINGERPRINT_GF3258) += gf_drv_tee.o

gf_drv_tee-objs := gf_spi.o platform.o netlink.o

endif
