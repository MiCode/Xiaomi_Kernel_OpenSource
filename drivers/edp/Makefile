GCOV_PROFILE := y

obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp.o
obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp_sysfs.o
obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp_dynamic_capping.o
obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp_reactive_capping.o
obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp_batmon_calc.o
ifdef CONFIG_DEBUG_FS
obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp_debug.o
endif
