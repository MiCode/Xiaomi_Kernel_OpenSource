#
# Makefile for the Synaptics TCM touchscreen driver.
#

# Each configuration option enables a list of files.
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-m += synaptics_tcm.o
synaptics_tcm-y += synaptics_tcm_i2c.o
synaptics_tcm-y += synaptics_tcm_core.o
synaptics_tcm-y += synaptics_tcm_touch.o
