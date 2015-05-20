# build this as an external kernel modules
$(eval $(call build_kernel_module,$(call my-dir)/,rtl8723bs,))
