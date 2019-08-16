#
# Makefile for the Synaptics TCM touchscreen driver.
#

# Each configuration option enables a list of files.

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-n += synaptics_tcm_spi.o
obj-y += synaptics_tcm_i2c.o
obj-y += synaptics_tcm_core.o
obj-y += synaptics_tcm_touch.o
obj-y += synaptics_tcm_device.o
obj-y += synaptics_tcm_testing.o
obj-y += synaptics_tcm_reflash.o
obj-y += synaptics_tcm_recovery.o
obj-n += synaptics_tcm_zeroflash.o
obj-y += synaptics_tcm_diagnostics.o
obj-n += synaptics_tcm_romboot.o
