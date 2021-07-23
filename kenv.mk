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
  mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
  current_dir := $(notdir $(patsubst %/,%,$(dir $(mkfile_path))))

  ifeq ($(KERNEL_TARGET_ARCH),arm64)
    ifeq ($(strip $(TARGET_KERNEL_USE_CLANG)),true)
      include $(current_dir)/build.config.mtk.aarch64
    else
      include $(current_dir)/build.config.mtk.aarch64.gcc
    endif
  else
    ifeq ($(strip $(TARGET_KERNEL_USE_CLANG)),true)
      include $(current_dir)/build.config.mtk.arm
    else
      $(error TARGET_KERNEL_USE_CLANG is not set)
    endif
  endif

  ARGS := CROSS_COMPILE=$(CROSS_COMPILE)
  ifneq ($(CLANG_TRIPLE),)
    ARGS += CLANG_TRIPLE=$(CLANG_TRIPLE)
  endif
  ifneq ($(LD),)
    ARGS += LD=$(LD)
  endif
  ifneq ($(LD_LIBRARY_PATH),)
    ARGS += LD_LIBRARY_PATH=$(KERNEL_ROOT_DIR)/$(LD_LIBRARY_PATH)
  endif
  ifneq ($(NM),)
    ARGS += NM=$(NM)
  endif
  ifneq ($(OBJCOPY),)
    ARGS += OBJCOPY=$(OBJCOPY)
  endif
  ifeq ("$(CC)", "gcc")
    CC :=
  endif

  ifneq ($(filter-out false,$(USE_CCACHE)),)
    CCACHE_EXEC ?= /usr/bin/ccache
    CCACHE_EXEC := $(abspath $(wildcard $(CCACHE_EXEC)))
  else
    CCACHE_EXEC :=
  endif
  ifneq ($(CCACHE_EXEC),)
    ifneq ($(CC),)
      ARGS += CCACHE_CPP2=yes CC='$(CCACHE_EXEC) $(CC)'
    endif
  else
    ifneq ($(CC),)
      ARGS += CC=$(CC)
    endif
  endif

  TARGET_KERNEL_CROSS_COMPILE := $(KERNEL_ROOT_DIR)/$(LINUX_GCC_CROSS_COMPILE_PREBUILTS_BIN)/$(CROSS_COMPILE)

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

    BUILT_KERNEL_TARGET := $(KERNEL_ZIMAGE_OUT).bin
    INSTALLED_KERNEL_TARGET := $(PRODUCT_OUT)/kernel
    INSTALLED_DTB_OVERLAY_TARGET := $(PRODUCT_OUT)/odmdtbo.img
    BUILT_DTB_OVERLAY_TARGET := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/dts/odmdtbo.img
    TARGET_KERNEL_CONFIG := $(KERNEL_OUT)/.config
    KERNEL_HEADERS_INSTALL := $(KERNEL_OUT)/usr
    KERNEL_CONFIG_FILE := $(KERNEL_DIR)/arch/$(KERNEL_TARGET_ARCH)/configs/$(word 1,$(KERNEL_DEFCONFIG))
    #KERNEL_CONFIG_MODULES := $(shell grep ^CONFIG_MODULES=y $(KERNEL_CONFIG_FILE))
    #KERNEL_MODULES_OUT := $(if $(filter /% ~%,$(TARGET_OUT)),,$(KERNEL_ROOT_DIR)/)$(TARGET_OUT)
    KERNEL_MODULES_OUT := $(TARGET_OUT_VENDOR)/lib/modules
    #KERNEL_MODULES_DEPS := $(if $(wildcard $(KERNEL_MODULES_OUT)/lib/modules/*.ko),$(wildcard $(KERNEL_MODULES_OUT)/lib/modules/*.ko),$(KERNEL_MODULES_OUT)/lib/modules)
    #KERNEL_MODULES_SYMBOLS_OUT := $(if $(filter /% ~%,$(TARGET_OUT_UNSTRIPPED)),,$(KERNEL_ROOT_DIR)/)$(TARGET_OUT_UNSTRIPPED)/system
    KERNEL_MAKE_OPTION := O=$(KERNEL_ROOT_OUT) ARCH=$(KERNEL_TARGET_ARCH) $(ARGS) ROOTDIR=$(KERNEL_ROOT_DIR)
    KERNEL_MAKE_PATH_OPTION := /usr/bin:/bin
    KERNEL_MAKE_OPTION += PATH=$(KERNEL_ROOT_DIR)/$(CLANG_PREBUILT_BIN):$(KERNEL_ROOT_DIR)/$(LINUX_GCC_CROSS_COMPILE_PREBUILTS_BIN):$(KERNEL_MAKE_PATH_OPTION):$$PATH
  ifdef MTK_DTBO_FEATURE
    KERNEL_MAKE_OPTION += MTK_DTBO_FEATURE=$(MTK_DTBO_FEATURE)
  endif
  else
    BUILT_KERNEL_TARGET := $(TARGET_PREBUILT_KERNEL)
  endif#TARGET_PREBUILT_KERNEL is empty
    KERNEL_MAKE_OPTION += PROJECT_DTB_NAMES='$(PROJECT_DTB_NAMES)'
endif#TARGET_NO_KERNEL
