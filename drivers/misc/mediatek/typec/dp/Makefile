ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
ifeq ($(CONFIG_MTK_USB_TYPEC_DP_MUX_V2), y)
obj-y += pi3dpx1205azlbex.o
endif
ifeq ($(CONFIG_MTK_USB_TYPEC_DP_MUX), y)
obj-y += usb_dp.o
endif
