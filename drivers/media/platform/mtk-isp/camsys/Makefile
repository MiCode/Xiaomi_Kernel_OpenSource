# SPDX-License-Identifier: GPL-2.0-only

ifneq ($(KERNELRELEASE),)
include Kbuild

else

# PATH
KROOT=$(shell expr match $(PWD) '\(.*/kernel-[0-9.]*/\)')
ROOT_DIR=$(realpath $(KROOT)/..)
OUT_DIR=$(PWD)/out

srctree=$(KROOT)
src=$(subst $(KROOT),,$(PWD))

# reference config file to fetch clang version
CONFIG=$(KROOT)build.config.common

CLANG_PREBUILT_BIN:=$(shell cat $(CONFIG) | grep CLANG_PREBUILT_BIN | cut -d'=' -f 2)
CLANG_PATH:=$(ROOT_DIR)/$(CLANG_PREBUILT_BIN:prebuilts-master%=prebuilts%)

#$(info CLANG_PREBUILT_BIN = $(CLANG_PREBUILT_BIN))
$(info CLANG_PATH = $(CLANG_PATH))

# Toolchain
TARGET_ARCH=arm64
CROSS_COMPILE:=aarch64-linux-gnu-
CROSS_COMPILE_COMPAT:=arm-linux-gnueabi-
CC=$(CLANG_PATH)/clang
LD=$(CLANG_PATH)/ld.lld
AR=$(CLANG_PATH)/llvm-ar
NM=$(CLANG_PATH)/llvm-nm
STRIP=$(CLANG_PATH)/llvm-strip
OBJCOPY=$(CLANG_PATH)/llvm-objcopy
OBJDUMP=$(CLANG_PATH)/llvm-objdump
READELF=$(CLANG_PATH)/llvm-readelf

HOSTCC=$(CLANG_PATH)/clang
HOSTAR=$(CLANG_PATH)/llvm-ar
HOSTLD=$(CLANG_PATH)/ld.lld

CFLAGS=-Wall LLVM=1 LLVM_IAS=1

### DEFINE ###
define kernel-make
+$(MAKE) -C $(KDIR) \
	ARCH=$(TARGET_ARCH) \
	CROSS_COMPILE=$(CROSS_COMPILE) \
	CROSS_COMPILE_COMPAT=$(CROSS_COMPILE_COMPAT) \
	CFLAGS=$(CFLAGS) \
	CC=$(CC) \
	LD=$(LD) \
	AR=$(AR) \
	STRIP=$(STRIP) \
	OBJCOPY=$(OBJCOPY) \
	OBJDUMP=$(OBJDUMP) \
	READELF=$(READELF) \
	HOSTCC=$(HOSTCC) \
	HOSTAR=$(HOSTAR) \
	HOSTLD=$(HOSTLD) \
	M=$(PWD)
endef


.PHONY: all build clean check-env

all: check-env build install
build:
	$(kernel-make) modules
install: build
	$(kernel-make) modules_install INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=$(OUT_DIR)
clean:
	$(kernel-make) clean
	# clean *.lds
	$(shell find -name "*.lds" -exec rm {} \;)
	# clean out
	$(shell rm -rf $(OUT_DIR))
check-env:
ifndef KDIR
	$(error KDIR (prebuilt kernel path) is undefined)
endif
$(info prebuilt kernel = $(KDIR))

endif
