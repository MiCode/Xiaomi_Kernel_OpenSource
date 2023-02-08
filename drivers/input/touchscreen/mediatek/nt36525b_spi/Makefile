#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.
# C3T code for HQ-224102 by chenzimo at 2022/08/25 start
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger/
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/misc/
# C3T code for HQ-224102 by chenzimo at 2022/08/25 end

obj-$(CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI) += nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o
