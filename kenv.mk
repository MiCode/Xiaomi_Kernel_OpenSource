# Copyright (C) 2017 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.

KERNEL_ENV_PATH := $(call my-dir)
KERNEL_ROOT_DIR := $(PWD)

define touch-kernel-image-timestamp
if [ -e $(1) ] && [ -e $(2) ] && cmp -s $(1) $(2); then \
 echo $(2) has no change;\
 mv -f $(1) $(2);\
else \
 rm -f $(1);\
fi
endef

define move-kernel-module-files
v=`cat $(2)/include/config/kernel.release`;\
for i in `grep -h '\.ko' /dev/null $(2)/.tmp_versions/*.mod`; do \
 o=`basename $$i`;\
 if [ -e $(1)/lib/modules/$$o ] && cmp -s $(1)/lib/modules/$$v/kernel/$$i $(1)/lib/modules/$$o; then \
  echo $(1)/lib/modules/$$o has no change;\
 else \
  echo Update $(1)/lib/modules/$$o;\
  mv -f $(1)/lib/modules/$$v/kernel/$$i $(1)/lib/modules/$$o;\
 fi;\
done
endef

define clean-kernel-module-dirs
rm -rf $(1)/lib/modules/$(if $(2),`cat $(2)/include/config/kernel.release`,*/)
endef

# '\\' in command is wrongly replaced to '\\\\' in kernel/out/arch/arm/boot/compressed/.piggy.xzkern.cmd
define fixup-kernel-cmd-file
if [ -e $(1) ]; then cp $(1) $(1).bak; sed -e 's/\\\\\\\\/\\\\/g' < $(1).bak > $(1); rm -f $(1).bak; fi
endef

ifneq ($(strip $(TARGET_NO_KERNEL)),true)
  KERNEL_DIR := $(KERNEL_ENV_PATH)

  ifeq ($(KERNEL_TARGET_ARCH),arm64)
    TARGET_KERNEL_CROSS_COMPILE ?= $(KERNEL_ROOT_DIR)/prebuilts/gcc/$(HOST_PREBUILT_TAG)/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
    TARGET_KERNEL_CLANG_COMPILE :=
    CC := $(TARGET_KERNEL_CROSS_COMPILE)gcc
    ifeq ($(strip $(TARGET_KERNEL_USE_CLANG)),true)
      CLANG_PATH=$(KERNEL_ROOT_DIR)/prebuilts/clang/host/linux-x86/clang-r353983c
      TARGET_KERNEL_CLANG_COMPILE := CLANG_TRIPLE=aarch64-linux-gnu-
      CC := $(CLANG_PATH)/bin/clang
    endif
  else
    TARGET_KERNEL_CROSS_COMPILE ?= $(KERNEL_ROOT_DIR)/prebuilts/gcc/$(HOST_PREBUILT_TAG)/arm/arm-linux-androideabi-4.9/bin/arm-linux-androidkernel-
    TARGET_KERNEL_CLANG_COMPILE :=
    CC := $(TARGET_KERNEL_CROSS_COMPILE)gcc
  endif

  ifeq ($(USE_CCACHE), true)
    CCACHE_EXEC ?= /usr/bin/ccache
    CCACHE_EXEC := $(abspath $(wildcard $(CCACHE_EXEC)))
  else
    CCACHE_EXEC :=
  endif
  ifneq ($(CCACHE_EXEC),)
    TARGET_KERNEL_CLANG_COMPILE += CCACHE_CPP2=yes CC='$(CCACHE_EXEC) $(CC)'
  else
    TARGET_KERNEL_CLANG_COMPILE += CC=$(CC)
  endif

  KERNEL_HOST_GCC_PREFIX := $(patsubst %strip,%,$(HOST_STRIP))
  ifeq (yes,yes)
  KERNEL_HOSTCC := $(KERNEL_ROOT_DIR)/$(KERNEL_HOST_GCC_PREFIX)gcc
  KERNEL_HOSTCXX := $(KERNEL_ROOT_DIR)/$(KERNEL_HOST_GCC_PREFIX)g++
  else
  KERNEL_HOST_GCC_TOOLCHAIN := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(HOST_STRIP)))))
  KERNEL_HOST_CLANG_FLAGS := --gcc-toolchain=$(KERNEL_ROOT_DIR)/$(KERNEL_HOST_GCC_TOOLCHAIN) --sysroot $(KERNEL_ROOT_DIR)/$(KERNEL_HOST_GCC_TOOLCHAIN)/sysroot $(patsubst -B%,-B$(KERNEL_ROOT_DIR)/%,$(filter -B%,$(SOONG_CLANG_HOST_GLOBAL_LDFLAGS))) $(patsubst -L%,-L$(KERNEL_ROOT_DIR)/%,$(filter -L%,$(SOONG_CLANG_HOST_GLOBAL_LDFLAGS)))
  KERNEL_HOSTCC := "$(KERNEL_ROOT_DIR)/$(CLANG) $(KERNEL_HOST_CLANG_FLAGS)"
  KERNEL_HOSTCXX := "$(KERNEL_ROOT_DIR)/$(CLANG_CXX) $(KERNEL_HOST_CLANG_FLAGS)"
  endif

  ifeq ($(wildcard $(TARGET_PREBUILT_KERNEL)),)
    KERNEL_OUT ?= $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
    KERNEL_ROOT_OUT := $(if $(filter /% ~%,$(KERNEL_OUT)),,$(KERNEL_ROOT_DIR)/)$(KERNEL_OUT)
    ifeq ($(KERNEL_TARGET_ARCH), arm64)
      ifeq ($(MTK_APPENDED_DTB_SUPPORT), yes)
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/Image.gz-dtb
      else
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/Image.gz
      endif
    else
      ifeq ($(MTK_APPENDED_DTB_SUPPORT), yes)
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/zImage-dtb
      else
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/zImage
      endif
    endif

    KERNEL_DTB_FILE := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/mtk.dtb
    BUILT_KERNEL_TARGET := $(KERNEL_ZIMAGE_OUT).bin
    INSTALLED_KERNEL_TARGET := $(PRODUCT_OUT)/kernel
    INSTALLED_DTB_OVERLAY_TARGET := $(PRODUCT_OUT)/odmdtbo.img
    BUILT_DTB_OVERLAY_TARGET := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/dts/odmdtbo.img
    TARGET_KERNEL_CONFIG := $(KERNEL_OUT)/.config
    KERNEL_HEADERS_INSTALL := $(KERNEL_OUT)/usr
    KERNEL_CONFIG_FILE := $(KERNEL_DIR)/arch/$(KERNEL_TARGET_ARCH)/configs/$(KERNEL_DEFCONFIG)
    #KERNEL_CONFIG_MODULES := $(shell grep ^CONFIG_MODULES=y $(KERNEL_CONFIG_FILE))
    #KERNEL_MODULES_OUT := $(if $(filter /% ~%,$(TARGET_OUT)),,$(KERNEL_ROOT_DIR)/)$(TARGET_OUT)
    #KERNEL_MODULES_DEPS := $(if $(wildcard $(KERNEL_MODULES_OUT)/lib/modules/*.ko),$(wildcard $(KERNEL_MODULES_OUT)/lib/modules/*.ko),$(KERNEL_MODULES_OUT)/lib/modules)
    #KERNEL_MODULES_SYMBOLS_OUT := $(if $(filter /% ~%,$(TARGET_OUT_UNSTRIPPED)),,$(KERNEL_ROOT_DIR)/)$(TARGET_OUT_UNSTRIPPED)/system
    KERNEL_MAKE_OPTION := O=$(KERNEL_ROOT_OUT) ARCH=$(KERNEL_TARGET_ARCH) CROSS_COMPILE=$(TARGET_KERNEL_CROSS_COMPILE) $(TARGET_KERNEL_CLANG_COMPILE) ROOTDIR=$(KERNEL_ROOT_DIR)
  ifdef MTK_DTBO_FEATURE
    KERNEL_MAKE_OPTION += MTK_DTBO_FEATURE=$(MTK_DTBO_FEATURE)
  endif
  ifdef KERNEL_HOSTCC
    KERNEL_MAKE_OPTION += HOSTCC=$(KERNEL_HOSTCC)
  endif
  ifdef KERNEL_HOSTCXX
    KERNEL_MAKE_OPTION += HOSTCXX=$(KERNEL_HOSTCXX)
  endif
  ifeq ($(KERNEL_TARGET_ARCH),arm64)
      ifeq ($(strip $(TARGET_KERNEL_USE_CLANG)),true)
          # for CONFIG_LTO_CLANG to find clang llvm-dis & llvm-ar & LLVMgold.so
          KERNEL_MAKE_OPTION += LD_LIBRARY_PATH=$(CLANG_PATH)/lib64:$$LD_LIBRARY_PATH
          KERNEL_MAKE_OPTION += PATH=$(CLANG_PATH)/bin/:$$PATH
      endif
  endif
  else
    BUILT_KERNEL_TARGET := $(TARGET_PREBUILT_KERNEL)
  endif#TARGET_PREBUILT_KERNEL is empty

endif#TARGET_NO_KERNEL
