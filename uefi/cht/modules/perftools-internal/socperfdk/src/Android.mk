LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := socperf1_2
LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE_TAGS := debug
include $(BUILD_EXTERNAL_KERNEL_MODULE)

ifneq ($(TARGET_BUILD_VARIANT),user)
$(eval $(call build_kernel_module,$(LOCAL_PATH),$(LOCAL_MODULE),))
endif
