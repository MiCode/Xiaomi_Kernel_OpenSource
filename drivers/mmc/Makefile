#
# Makefile for the kernel mmc device drivers.
#
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include

obj-$(CONFIG_MMC)		+= core/
obj-$(subst m,y,$(CONFIG_MMC))	+= host/
