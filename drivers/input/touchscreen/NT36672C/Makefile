#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.

ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
obj-m += nt36672c.o
nt36672c-y += nt36xxx.o
nt36672c-y += nt36xxx_fw_update.o
nt36672c-y += nt36xxx_ext_proc.o
nt36672c-y += nt36xxx_mp_ctrlram.o
