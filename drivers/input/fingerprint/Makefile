#
# Makefile for all kinds of Fingerprint
#

obj-y += goodix/

obj-$(CONFIG_FPC_FINGERPRINT) += fpc/fpc1022_tee.o
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/270/tz_driver/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/270/common/include/
