#
# Copyright (C) 2009-2011 The Android-x86 Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
ifeq ($(LINUX_KERNEL_VERSION),kernel-3.10)
ifneq ($(strip $(MTK_EMULATOR_SUPPORT)),yes)
ifneq ($(strip $(MTK_PROJECT_NAME)),)

ifneq ($(wildcard $(call my-dir)/arch/$(TARGET_ARCH)/configs/$(KERNEL_DEFCONFIG)),)

KERNEL_DIR := $(call my-dir)
ROOTDIR := $(abspath $(TOP))

ifneq ($(filter /% ~%,$(OUT_DIR)),)
KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
else
KERNEL_OUT := $(ROOTDIR)/$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
endif

ifeq ($(TARGET_ARCH), arm64)
  ifeq ($(MTK_APPENDED_DTB_SUPPORT), yes)
    TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/Image.gz-dtb
  else
    TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/Image.gz
  endif
else
  ifeq ($(MTK_APPENDED_DTB_SUPPORT), yes)
    TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/zImage-dtb
  else
    TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/zImage
  endif
endif
TARGET_PREBUILT_KERNEL_BIN := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/zImage.bin

TARGET_KERNEL_CONFIG := $(KERNEL_OUT)/.config
KERNEL_HEADERS_INSTALL := $(KERNEL_OUT)/usr

ifneq ($(strip $(TARGET_NO_KERNEL)),true)
  INSTALLED_KERNEL_TARGET := $(PRODUCT_OUT)/kernel
else
  INSTALLED_KERNEL_TARGET :=
endif

ifeq ($(KERNEL_CROSS_COMPILE),)
ifeq ($(TARGET_ARCH), arm64)
KERNEL_CROSS_COMPILE := aarch64-linux-android-
else
KERNEL_CROSS_COMPILE := arm-eabi-
endif
endif

$(KERNEL_OUT):
	mkdir -p $@

.PHONY: kernel kernel-defconfig kernel-menuconfig clean-kernel
kernel-menuconfig: | $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_OUT) ARCH=$(TARGET_ARCH) MTK_TARGET_PROJECT=${MTK_TARGET_PROJECT} CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) ROOTDIR=$(ROOTDIR) menuconfig

kernel-savedefconfig: | $(KERNEL_OUT)
	cp $(TARGET_KERNEL_CONFIG) $(KERNEL_DIR)/arch/$(TARGET_ARCH)/configs/$(KERNEL_DEFCONFIG)

$(TARGET_PREBUILT_KERNEL): kernel
	@echo Done kernel

$(TARGET_KERNEL_CONFIG) kernel-defconfig: $(KERNEL_DIR)/arch/$(TARGET_ARCH)/configs/$(KERNEL_DEFCONFIG) | $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_OUT) ARCH=$(TARGET_ARCH) MTK_TARGET_PROJECT=${MTK_TARGET_PROJECT} CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) ROOTDIR=$(ROOTDIR) $(KERNEL_DEFCONFIG)
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_OUT) ARCH=$(TARGET_ARCH) MTK_TARGET_PROJECT=${MTK_TARGET_PROJECT} CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) ROOTDIR=$(ROOTDIR) oldconfig

$(KERNEL_HEADERS_INSTALL): $(TARGET_KERNEL_CONFIG) | $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_OUT) ARCH=$(TARGET_ARCH) MTK_TARGET_PROJECT=${MTK_TARGET_PROJECT} CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) ROOTDIR=$(ROOTDIR) headers_install

kernel: $(TARGET_KERNEL_CONFIG) $(KERNEL_HEADERS_INSTALL) | $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_OUT) ARCH=$(TARGET_ARCH) MTK_TARGET_PROJECT=${MTK_TARGET_PROJECT} CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) ROOTDIR=$(ROOTDIR) 

$(INSTALLED_KERNEL_TARGET): kernel

ifeq ($(strip $(MTK_HEADER_SUPPORT)),yes)
$(TARGET_PREBUILT_KERNEL_BIN): $(TARGET_PREBUILT_KERNEL) | $(HOST_OUT_EXECUTABLES)/mkimage
	$(HOST_OUT_EXECUTABLES)/mkimage $< KERNEL 0xffffffff > $@ 	

$(INSTALLED_KERNEL_TARGET): $(TARGET_PREBUILT_KERNEL_BIN) | $(ACP)
	$(copy-file-to-target)
else
$(INSTALLED_KERNEL_TARGET): $(TARGET_PREBUILT_KERNEL) | $(ACP)
	$(copy-file-to-target)
endif

clean-kernel:
	@rm -rf $(KERNEL_OUT)

droid: check-kernel-config
check-mtk-config: check-kernel-config
check-kernel-config:
	-python device/mediatek/build/build/tools/check_kernel_config.py -c $(MTK_TARGET_PROJECT_FOLDER)/ProjectConfig.mk -k $(KERNEL_DIR)/arch/$(TARGET_ARCH)/configs/$(KERNEL_DEFCONFIG) -p $(MTK_PROJECT_NAME)

endif
endif
endif
endif # ifeq ($(LINUX_KERNEL_VERSION),kernel-3.10)

