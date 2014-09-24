#
# Makefile for FDP NFC device driver
#

fdp_nfc-objs = fdp_main.o fdp_custom.o
obj-$(CONFIG_ANDROID_NFC_FDP) += fdp_nfc.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG