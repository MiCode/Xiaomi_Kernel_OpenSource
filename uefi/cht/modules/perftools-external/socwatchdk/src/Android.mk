# Build as an external kernel module

ifneq ($(TARGET_BUILD_VARIANT),user)
$(eval $(call build_kernel_module,$(call my-dir),swdrv_external))
endif
