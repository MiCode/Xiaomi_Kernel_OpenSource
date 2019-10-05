obj-y += ctrl/
ifneq (,$(filter y,$(CONFIG_MTK_MDDP_USB_SUPPORT) $(CONFIG_MTK_MDDP_WH_SUPPORT)))
subdir-ccflags-y += -DMDDP_TETHERING_SUPPORT
obj-y += filter/
# Data usage
obj-y += usage/
endif
