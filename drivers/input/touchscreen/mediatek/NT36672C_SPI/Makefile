#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.

ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
obj-$(CONFIG_TOUCHSCREEN_MTK_NT36672C_SPI) += nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o
