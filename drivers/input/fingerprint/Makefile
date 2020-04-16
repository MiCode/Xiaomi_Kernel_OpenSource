#
# Makefile for all kinds of Fingerprint
#

obj-$(CONFIG_GOODIX_FINGERPRINT) += goodix/
obj-$(CONFIG_FPC_FINGERPRINT) += fpc/fpc1022_tee.o
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/300/tz_driver/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/300/common/include/
#obj-$(CONFIG_FPC_FINGERPRINT) += fpc/
