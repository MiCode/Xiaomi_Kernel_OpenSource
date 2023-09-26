
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/inc

ifeq ($(CONFIG_AGATE_CHARGER),y)
obj-$(CONFIG_TCPC_CLASS) += tcpc_agate/
else ifeq ($(CONFIG_ARES_CHARGER),y)
obj-$(CONFIG_TCPC_CLASS) += tcpc_ares/
else ifeq ($(CONFIG_PISSARRO_CHARGER),y)
obj-$(CONFIG_TCPC_CLASS) += tcpc_pissarro/
else
obj-$(CONFIG_TCPC_CLASS) += tcpc/
endif

obj-$(CONFIG_MTK_USB_TYPEC_U3_MUX) += u3_switch/
obj-$(CONFIG_MTK_USB_TYPEC_DP_MUX) += dp/
