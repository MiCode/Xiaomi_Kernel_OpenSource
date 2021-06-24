# common KOs
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += v1/

# platform dependent  KOs
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

ifeq ($(CONFIG_MTK_LPM_MT6893),m)
ifneq ($(wildcard $(srctree)/$(src)/mt6893),)
obj-$(CONFIG_MTK_LPM_MT6893) += mt6893/
endif
endif

ifeq ($(CONFIG_MTK_LPM_MT6983),m)
ifneq ($(wildcard $(srctree)/$(src)/6983),)
obj-$(CONFIG_MTK_LPM_MT6893) += 6983/
endif
endif
