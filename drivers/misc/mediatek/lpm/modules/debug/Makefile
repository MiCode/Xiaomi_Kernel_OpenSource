# common KOs
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += v1/

ifeq ($(CONFIG_MTK_LPM_MT6983),m)
ifneq ($(wildcard $(srctree)/$(src)/mt6983),)
obj-$(CONFIG_MTK_LPM_MT6983) += mt6983/
endif
endif

ifeq ($(CONFIG_MTK_LPM_MT6985),m)
ifneq ($(wildcard $(srctree)/$(src)/mt6985),)
obj-$(CONFIG_MTK_LPM_MT6985) += mt6985/
endif
endif

ifeq ($(CONFIG_MTK_LPM_MT6886),m)
ifneq ($(wildcard $(srctree)/$(src)/mt6886),)
obj-$(CONFIG_MTK_LPM_MT6886) += mt6886/
endif
endif
