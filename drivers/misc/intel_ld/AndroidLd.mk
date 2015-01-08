# This makefile is included from vendor/intel/*/AndroidBoard.mk.
$(eval $(call build_kernel_module,$(call my-dir),lnp_ldisc, CONFIG_INTEL_ST_LD=m))

