#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.
# Huaqin add for HQ-131657 by liunianliang at 2021/06/03 start
ccflags-$(CONFIG_TOUCHSCREEN_NT36672C) += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-$(CONFIG_TOUCHSCREEN_NT36672C) += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
# Huaqin add for HQ-131657 by liunianliang at 2021/06/03 end

obj-$(CONFIG_TOUCHSCREEN_NT36672C) += nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o
