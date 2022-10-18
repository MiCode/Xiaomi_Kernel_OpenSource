# Standalone camera UAPI header android target
LOCAL_PATH := $(call my-dir)
# Path variable for other modules to include for compilation
LOCAL_EXPORT_CAMERA_UAPI_INCLUDE := $(LOCAL_PATH)/camera/

CAMERA_HEADERS := $(call all-subdir-named-files,*.h)
KERNEL_SCRIPTS := $(shell pwd)/kernel/msm-$(TARGET_KERNEL_VERSION)/scripts

include $(CLEAR_VARS)
LOCAL_MODULE := camera-uapi
