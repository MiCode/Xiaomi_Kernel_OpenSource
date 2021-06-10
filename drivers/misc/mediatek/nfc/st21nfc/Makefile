#
# Makefile for ST21NFC
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
obj-$(CONFIG_NFC_CHIP_SUPPORT) +=st21nfc.o
ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
