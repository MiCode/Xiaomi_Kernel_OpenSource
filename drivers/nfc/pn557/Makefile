#
# Makefile for nfc devices
#
pn557-objs = nfc.o sn1xx.o pn8xt.o
obj-y += pn557.o
ccflags-$(CONFIG_NXP_NFC_SN1XX)  := -DNFC_PLATFORM=sn1xx
ccflags-$(CONFIG_NXP_NFC_PN8XT)  := -DNFC_PLATFORM=pn8xt
