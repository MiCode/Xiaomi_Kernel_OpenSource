# common KOs
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += v1/

# platform dependent  KOs

ifeq ($(CONFIG_MTK_LPM_MT6789),m)
ifneq ($(wildcard $(srctree)/$(src)/mt6789),)
obj-$(CONFIG_MTK_LPM_MT6789) += mt6789/
endif
endif

ifeq ($(CONFIG_MTK_LPM_MT6873),m)
ifneq ($(wildcard $(srctree)/$(src)/mt6873),)
obj-$(CONFIG_MTK_LPM_MT6873) += mt6873/
endif
endif
ifeq ($(CONFIG_MTK_LPM_MT6853),m)
ifneq ($(wildcard $(srctree)/$(src)/mt6853),)
obj-$(CONFIG_MTK_LPM_MT6853) += mt6853/
endif
endif

ifeq ($(CONFIG_MTK_LPM_MT6855),m)
ifneq ($(wildcard $(srctree)/$(src)/mt6855),)
obj-$(CONFIG_MTK_LPM_MT6855) += mt6855/
endif
endif

ifeq ($(CONFIG_MTK_LPM_MT6879),m)
ifneq ($(wildcard $(srctree)/$(src)/mt6879),)
obj-$(CONFIG_MTK_LPM_MT6879) += mt6879/
endif
endif

ifeq ($(CONFIG_MTK_LPM_MT6893),m)
ifneq ($(wildcard $(srctree)/$(src)/mt6893),)
obj-$(CONFIG_MTK_LPM_MT6893) += mt6893/
endif
endif

ifeq ($(CONFIG_MTK_LPM_MT6895),m)
ifneq ($(wildcard $(srctree)/$(src)/mt6895),)
obj-$(CONFIG_MTK_LPM_MT6895) += mt6895/
endif
endif

ifeq ($(CONFIG_MTK_LPM_MT6983),m)
ifneq ($(wildcard $(srctree)/$(src)/mt6983),)
obj-$(CONFIG_MTK_LPM_MT6983) += mt6983/
endif
endif

