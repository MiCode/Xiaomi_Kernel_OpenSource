#
# Makefile for the Novatek NT36523 touchscreen driver.
#

# Each configuration option enables a list of files.
ifneq ($(KERNELRELEASE),)
	obj-$(CONFIG_TOUCHSCREEN_NT36523_HOSTDL_SPI) += nt36523_touch.o
	nt36523_touch-y := nt36xxx.o \
		nt36xxx_fw_update.o \
		nt36xxx_ext_proc.o \
		nt36xxx_mp_ctrlram.o
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
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) REAL_CC=$(REAL_CC) CLANG_TRIPLE=aarch64-lin    ux-gnu- AR=$(AR) LLVM_NM=$(LLVM_NM) LD=$(LD) -C $(KDIR) M=$(PWD) modules
clean:
	@rm -rf *.order *.symvers* .tmp_versions
	@find -name "*.o*" -o -name "*.mod*" -o -name "*.ko*" | xargs rm -f
endif
