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

LOCAL_PATH := $(call my-dir)

ifeq ($(notdir $(LOCAL_PATH)),$(strip $(LINUX_KERNEL_VERSION)))
ifneq ($(strip $(TARGET_NO_KERNEL)),true)
include $(LOCAL_PATH)/kenv.mk

ifeq ($(wildcard $(TARGET_PREBUILT_KERNEL)),)
KERNEL_MAKE_DEPENDENCIES := $(shell find $(KERNEL_DIR) -name .git -prune -o -type f | sort)
KERNEL_MAKE_DEPENDENCIES := $(filter-out %/.git %/.gitignore %/.gitattributes,$(KERNEL_MAKE_DEPENDENCIES))

$(TARGET_KERNEL_CONFIG): PRIVATE_DIR := $(KERNEL_DIR)
$(TARGET_KERNEL_CONFIG): $(KERNEL_CONFIG_FILE) $(LOCAL_PATH)/Android.mk
$(TARGET_KERNEL_CONFIG): $(KERNEL_MAKE_DEPENDENCIES)
	$(hide) mkdir -p $(dir $@)
	$(PREBUILT_MAKE_PREFIX)$(MAKE) -C $(PRIVATE_DIR) $(KERNEL_MAKE_OPTION) $(KERNEL_DEFCONFIG)

$(BUILT_DTB_OVERLAY_TARGET): $(KERNEL_ZIMAGE_OUT)

.KATI_RESTAT: $(KERNEL_ZIMAGE_OUT)
$(KERNEL_ZIMAGE_OUT): PRIVATE_DIR := $(KERNEL_DIR)
$(KERNEL_ZIMAGE_OUT): $(TARGET_KERNEL_CONFIG) $(KERNEL_MAKE_DEPENDENCIES)
	$(hide) mkdir -p $(dir $@)
	$(PREBUILT_MAKE_PREFIX)$(MAKE) -C $(PRIVATE_DIR) $(KERNEL_MAKE_OPTION)
	$(hide) $(call fixup-kernel-cmd-file,$(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/compressed/.piggy.xzkern.cmd)
	# check the kernel image size
	python device/mediatek/build/build/tools/check_kernel_size.py $(KERNEL_OUT) $(KERNEL_DIR)

ifeq ($(strip $(MTK_HEADER_SUPPORT)), yes)
$(BUILT_KERNEL_TARGET): $(KERNEL_ZIMAGE_OUT) $(TARGET_KERNEL_CONFIG) $(LOCAL_PATH)/Android.mk | $(HOST_OUT_EXECUTABLES)/mkimage$(HOST_EXECUTABLE_SUFFIX)
	$(hide) $(HOST_OUT_EXECUTABLES)/mkimage$(HOST_EXECUTABLE_SUFFIX) $< KERNEL 0xffffffff > $@
else
$(BUILT_KERNEL_TARGET): $(KERNEL_ZIMAGE_OUT) $(TARGET_KERNEL_CONFIG) $(LOCAL_PATH)/Android.mk | $(ACP)
	$(copy-file-to-target)
endif

$(TARGET_PREBUILT_KERNEL): $(BUILT_KERNEL_TARGET) $(LOCAL_PATH)/Android.mk | $(ACP)
	$(copy-file-to-new-target)

endif#TARGET_PREBUILT_KERNEL is empty

$(INSTALLED_KERNEL_TARGET): $(BUILT_KERNEL_TARGET) $(LOCAL_PATH)/Android.mk | $(ACP)
	$(copy-file-to-target)

.PHONY: kernel save-kernel kernel-savedefconfig kernel-menuconfig menuconfig-kernel savedefconfig-kernel clean-kernel
kernel: $(INSTALLED_KERNEL_TARGET)
save-kernel: $(TARGET_PREBUILT_KERNEL)

kernel-savedefconfig: $(TARGET_KERNEL_CONFIG)
	cp $(TARGET_KERNEL_CONFIG) $(KERNEL_CONFIG_FILE)

kernel-menuconfig:
	$(hide) mkdir -p $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) menuconfig

menuconfig-kernel savedefconfig-kernel:
	$(hide) mkdir -p $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(patsubst %config-kernel,%config,$@)

clean-kernel:
	$(hide) rm -rf $(KERNEL_OUT) $(KERNEL_MODULES_OUT) $(INSTALLED_KERNEL_TARGET)
	$(hide) rm -f $(INSTALLED_DTB_OVERLAY_TARGET)


.PHONY: check-kernel-config check-kernel-dotconfig check-mtk-config
droid: check-kernel-config check-kernel-dotconfig
check-mtk-config: check-kernel-config check-kernel-dotconfig
check-kernel-config: PRIVATE_COMMAND := $(if $(wildcard device/mediatek/build/build/tools/check_kernel_config.py),$(if $(filter yes,$(DISABLE_MTK_CONFIG_CHECK)),-)python device/mediatek/build/build/tools/check_kernel_config.py -c $(MTK_TARGET_PROJECT_FOLDER)/ProjectConfig.mk -k $(KERNEL_CONFIG_FILE) -p $(MTK_PROJECT_NAME))
check-kernel-config:
	$(PRIVATE_COMMAND)

check-kernel-dotconfig: PRIVATE_COMMAND := $(if $(wildcard device/mediatek/build/build/tools/check_kernel_config.py),$(if $(filter yes,$(DISABLE_MTK_CONFIG_CHECK)),-)python device/mediatek/build/build/tools/check_kernel_config.py -c $(MTK_TARGET_PROJECT_FOLDER)/ProjectConfig.mk -k $(TARGET_KERNEL_CONFIG) -p $(MTK_PROJECT_NAME))
check-kernel-dotconfig: $(TARGET_KERNEL_CONFIG)
	$(PRIVATE_COMMAND)

### DTB
ifdef BOARD_PREBUILT_DTBIMAGE_DIR
INSTALLED_MTK_DTB_TARGET := $(BOARD_PREBUILT_DTBIMAGE_DIR)/mtk_dtb
$(shell if [ ! -f $(INSTALLED_MTK_DTB_TARGET) ]; then mkdir -p $(dir $(INSTALLED_MTK_DTB_TARGET)); touch $(INSTALLED_MTK_DTB_TARGET);fi)
$(INSTALLED_MTK_DTB_TARGET): $(INSTALLED_KERNEL_TARGET)
	@mkdir -p $(dir $@)
	@cp -f $(KERNEL_DTB_FILE) $@
endif

endif#TARGET_NO_KERNEL
endif#LINUX_KERNEL_VERSION
