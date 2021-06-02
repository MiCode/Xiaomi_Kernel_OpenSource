#
# Makefile for trusty components
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public

obj-$(CONFIG_MTK_ENABLE_GENIEZONE)	+= trusty.o trusty-irq.o trusty-mem.o \
					   trusty-virtio.o trusty-ipc.o \
					   smcnr-table.o
obj-$(CONFIG_TRUSTY_FIQ)		+= trusty-fiq.o
obj-$(CONFIG_TRUSTY_FIQ_ARM)		+= trusty-fiq-arm.o
obj-$(CONFIG_TRUSTY_FIQ_ARM64)		+= trusty-fiq-arm64.o \
					   trusty-fiq-arm64-glue.o
obj-$(CONFIG_GZ_LOG)			+= gz-log.o
obj-$(CONFIG_MT_GZ_TRUSTY_DEBUGFS)	+= trusty-debugfs.o
