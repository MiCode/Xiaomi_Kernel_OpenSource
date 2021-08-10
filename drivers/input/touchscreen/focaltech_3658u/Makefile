ifneq ($(KERNELRELEASE),)
    obj-$(CONFIG_TOUCHSCREEN_FOCALTECH_3658U) += focaltech_touch.o
    focaltech_touch-y := focaltech_core.o \
                         focaltech_ex_fun.o \
                         focaltech_ex_mode.o \
                         focaltech_gesture.o \
                         focaltech_esdcheck.o \
                         focaltech_spi.o \
                         focaltech_point_report_check.o \
                         focaltech_flash.o \
                         focaltech_flash/focaltech_upgrade_ft3658u.o \
                         focaltech_test/focaltech_test.o \
                         focaltech_test/focaltech_test_ini.o \
                         focaltech_test/supported_ic/focaltech_test_ft3658u.o
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
