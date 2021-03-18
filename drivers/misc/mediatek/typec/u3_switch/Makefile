ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
ifeq ($(CONFIG_MTK_USB_TYPEC_U3_MUX_V2), y)
obj-y += mux.o
obj-y += mux_switch.o
obj-y += fusb340.o
obj-y += ps5169.o
obj-y += ptn36241g.o
else
obj-y += usb_switch.o
endif
