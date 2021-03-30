ifneq ($(KERNELRELEASE),)
        obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5) += cyttsp5.o
        cyttsp5-y := cyttsp5_core.o cyttsp5_mt_common.o cyttsp5_platform.o \
                     cyttsp5_mtb.o cyttsp5_devtree.o
        cyttsp5-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_BUTTON) += cyttsp5_btn.o
        cyttsp5-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_PROXIMITY) += cyttsp5_proximity.o
        # obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT) += cyttsp5_devtree.o
        obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_I2C) += cyttsp5_i2c.o
        obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_SPI) += cyttsp5_spi.o
        obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEBUG_MDL) += cyttsp5_debug.o
        obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_LOADER) += cyttsp5_loader.o
        obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICE_ACCESS) += cyttsp5_device_access.o
else
    KDIR = $(OUT)/obj/KERNEL_OBJ
    CROSS_COMPILE = $(ANDROID_TOOLCHAIN)/aarch64-linux-android-
    CLANG = $(ANDROID_BUILD_TOP)/prebuilts/clang/host/linux-x86/clang-r370808
    REAL_CC = $(CLANG)/bin/clang
    AR = $(CLANG)/bin/llvm-ar
    LLVM_NM = $(CLANG)/bin/llvm-nm
    LD = $(CLANG)/bin/ld.lld

.PHONY: clean

default:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) REAL_CC=$(REAL_CC) CLANG_TRIPLE=aarch64-linux-gnu- AR=$(AR) LLVM_NM=$(LLVM_NM) LD=$(LD) -C $(KDIR) M=$(PWD) modules
clean:
	@rm -rf *.order *.symvers* .tmp_versions
	@find -name "*.o*" -o -name "*.mod*" -o -name "*.ko*" | xargs rm -f
endif




