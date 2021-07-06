#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

obj-$(CONFIG_TOUCHSCREEN_MTK_NT36672) += nt36672.o nt36672_fw_update.o nt36672_ext_proc.o nt36672_mp_ctrlram.o
