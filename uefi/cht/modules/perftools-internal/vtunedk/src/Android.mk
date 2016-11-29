LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := sep3_15
LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE_TAGS := eng
include $(BUILD_EXTERNAL_KERNEL_MODULE)

ifeq ($(TARGET_BUILD_VARIANT),eng)
$(eval $(call build_kernel_module,$(LOCAL_PATH),$(LOCAL_MODULE),))
endif

include $(call all-makefiles-under,$(LOCAL_PATH))
