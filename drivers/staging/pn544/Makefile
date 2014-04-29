#
# Makefile for nfc devices
#

pn544_nfc-objs = pn544.o

obj-$(CONFIG_ANDROID_NFC_PN544)	+= pn544_nfc.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
