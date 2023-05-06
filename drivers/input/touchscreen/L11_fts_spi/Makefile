ifneq ($(KERNELRELEASE),)
    obj-$(CONFIG_TOUCHSCREEN_ST_FTS_V521_SPI) := fts_touch_spi.o
    fts_touch_spi-y := fts_lib/ftsCompensation.o \
			fts_lib/ftsCore.o \
			fts_lib/ftsError.o \
			fts_lib/ftsFrame.o \
			fts_lib/ftsIO.o \
			fts_lib/ftsTest.o \
			fts_lib/ftsTool.o \
			fts_lib/ftsFlash.o \
			fts_lib/ftsGesture.o \
			fts_proc.o \
			fts.o
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
