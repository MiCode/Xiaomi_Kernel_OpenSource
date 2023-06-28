# Makefile for the Synaptics TCM touchscreen driver.
# Each configuration option enables a list of files.
# Makefile for the Synaptics TCM touchscreen driver.
# Each configuration option enables a list of files.
ifneq ($(KERNELRELEASE),)
		obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_SPI)	+= synaptics_tcm_spi.o
		obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_CORE)	+= synaptics_tcm_core_module.o
		synaptics_tcm_core_module-y := synaptics_tcm_core.o synaptics_tcm_touch.o
		obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_REFLASH)	+= synaptics_tcm_reflash.o
		obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_DEVICE)	+= synaptics_tcm_device.o
		obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_TESTING) += synaptics_tcm_testing.o
		obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_DIAGNOSTIC) += synaptics_tcm_diagnostics.o

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
	@rm -rf *.o* *.order *.symvers *.mod* .*.o.cmd .*.mod.o.cmd .*.ko.cmd .tmp_versions *.ko
	@find -name *.o | xargs rm -f
endif
