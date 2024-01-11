#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.
#ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
#obj-$(CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI) += nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/misc/mediatek/scp/include/

obj-m += nt36528_spi.o
nt36528_spi-y	+= nt36xxx.o
nt36528_spi-y	+= nt36xxx_fw_update.o
nt36528_spi-y	+= nt36xxx_ext_proc.o
nt36528_spi-y	+= nt36xxx_mp_ctrlram.o

