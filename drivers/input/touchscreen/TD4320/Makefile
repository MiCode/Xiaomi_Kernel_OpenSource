#
# Makefile for the Synaptics TCM touchscreen driver.
#

# Each configuration option enables a list of files.

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
obj-m += td4320.o
td4320-n += synaptics_tcm_spi.o
td4320-y += synaptics_tcm_i2c.o
td4320-y += synaptics_tcm_core.o
td4320-y += synaptics_tcm_touch.o
td4320-y += synaptics_tcm_device.o
td4320-y += synaptics_tcm_testing.o
td4320-y += synaptics_tcm_reflash.o
td4320-y += synaptics_tcm_recovery.o
td4320-n += synaptics_tcm_zeroflash.o
td4320-y += synaptics_tcm_diagnostics.o
td4320-n += synaptics_tcm_romboot.o
