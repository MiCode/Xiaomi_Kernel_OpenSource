#
# Makefile for trusty components
#

obj-$(CONFIG_TRUSTY)		+= trusty.o
obj-$(CONFIG_TRUSTY)		+= trusty-irq.o
obj-$(CONFIG_TRUSTY_FIQ)	+= trusty-fiq.o
obj-$(CONFIG_TRUSTY_FIQ_ARM)	+= trusty-fiq-arm.o
obj-$(CONFIG_TRUSTY_FIQ_ARM64)	+= trusty-fiq-arm64.o trusty-fiq-arm64-glue.o
ifeq ($(CONFIG_MTK_ENABLE_GENIEZONE),y)
obj-$(CONFIG_TRUSTY_LOG)	+= gz-log.o
else
obj-$(CONFIG_TRUSTY_LOG)	+= trusty-log.o
endif
obj-$(CONFIG_TRUSTY)		+= trusty-mem.o
obj-$(CONFIG_TRUSTY_VIRTIO)	+= trusty-virtio.o
obj-$(CONFIG_TRUSTY_VIRTIO_IPC)	+= trusty-ipc.o
