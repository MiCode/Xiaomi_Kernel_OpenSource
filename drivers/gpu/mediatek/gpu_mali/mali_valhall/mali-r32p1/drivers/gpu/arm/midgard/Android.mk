LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := mali_kbase_$(LOCAL_MTK_PLATFORM).ko
LOCAL_PROPRIETARY_MODULE := true
LOCAL_INIT_RC := init.mali_kbase.rc
include $(MTK_KERNEL_MODULE)

$(info [GPU][DDK] GPU_OPTS = $(GPU_OPTS))
$(linked_module): OPTS += $(GPU_OPTS)
