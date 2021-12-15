ifeq ($(CONFIG_MACH_MT6877),y)
obj-y += v1/

else ifeq ($(CONFIG_MACH_MT6893),y)
obj-y += v1/

else ifeq ($(CONFIG_MACH_MT6833),y)
obj-y += v1/

else
obj-$(CONFIG_MFD_MT6360_PMU) += pmu/
obj-$(CONFIG_MT6360_PMIC) += pmic/
obj-$(CONFIG_MT6360_LDO) += ldo/
endif
