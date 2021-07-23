#
# Makefile for all kinds of Fingerprint
#

obj-$(CONFIG_GOODIX_FINGERPRINT) += goodix/
obj-$(CONFIG_GOODIX_CAP_FINGERPRINT) += goodix_cap/
obj-$(CONFIG_FPC_FINGERPRINT) += fpc/fpc1022_tee.o
obj-$(CONFIG_GOODIX_5658_FINGERPRINT) += goodix_5658/
obj-$(CONFIG_FPC_1511_FINGERPRINT) += fpc_1511/fpc1022_tee.o
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/300/tz_driver/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/300/common/include/

