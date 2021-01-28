# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2019 MediaTek Inc.

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
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/Image.gz
        KERNEL_DTB_TARGET := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/dts/mediatek/$(TARGET_BOARD_PLATFORM).dtb
    else
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/zImage
        KERNEL_DTB_TARGET := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/dts/$(TARGET_BOARD_PLATFORM).dtb
    endif

    INSTALLED_MTK_DTB_TARGET := $(BOARD_PREBUILT_DTBIMAGE_DIR)/mtk_dtb
    BUILT_KERNEL_TARGET := $(KERNEL_ZIMAGE_OUT).bin
    INSTALLED_KERNEL_TARGET := $(PRODUCT_OUT)/kernel
    TARGET_KERNEL_CONFIG := $(KERNEL_OUT)/.config
    KERNEL_CONFIG_FILE := $(KERNEL_DIR)/arch/$(KERNEL_TARGET_ARCH)/configs/$(word 1,$(KERNEL_DEFCONFIG))
    KERNEL_MAKE_OPTION := O=$(KERNEL_ROOT_OUT) ARCH=$(KERNEL_TARGET_ARCH) $(ARGS) ROOTDIR=$(KERNEL_ROOT_DIR)
    KERNEL_MAKE_PATH_OPTION := /usr/bin:/bin
    KERNEL_MAKE_OPTION += PATH=$(KERNEL_ROOT_DIR)/$(CLANG_PREBUILT_BIN):$(KERNEL_ROOT_DIR)/$(LINUX_GCC_CROSS_COMPILE_PREBUILTS_BIN):$(KERNEL_MAKE_PATH_OPTION):$$PATH
  else
    BUILT_KERNEL_TARGET := $(TARGET_PREBUILT_KERNEL)
  endif #TARGET_PREBUILT_KERNEL is empty
    KERNEL_MAKE_OPTION += PROJECT_DTB_NAMES=$(PROJECT_DTB_NAMES)
endif #TARGET_NO_KERNEL
