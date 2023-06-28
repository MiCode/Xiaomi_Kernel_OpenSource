#
# Makefile for nfc devices
#
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/clkbuf_v1/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/clkbuf_v1/$(MTK_PLATFORM)/

pn557-objs = nfc.o sn1xx.o pn8xt.o
obj-y += pn557.o
ccflags-$(CONFIG_NXP_NFC_SN1XX)  := -DNFC_PLATFORM=sn1xx
ccflags-$(CONFIG_NXP_NFC_PN8XT)  := -DNFC_PLATFORM=pn8xt


