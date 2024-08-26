#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
# N19A code for HQ-359934 by p-huangyunbiao at 2023/12/19 start
ccflags-y += -I$(srctree)/drivers/input/touchscreen/tpd_notify/
# N19A code for HQ-353617 by p-huangyunbiao at 2024/02/19 start
ccflags-y += -I$(srctree)/drivers/
# N19A code for HQ-353617 by p-huangyunbiao at 2024/02/19 end
obj-$(CONFIG_TOUCHSCREEN_NT36XXX_HOSTDL_SPI) += nt36672c_spi.o
# N19A code for HQ-359934 by p-huangyunbiao at 2023/12/19 end
nt36672c_spi-y := \
				nt36xxx.o \
				nt36xxx_fw_update.o \
				nt36xxx_ext_proc.o \
				nt36xxx_mp_ctrlram.o
