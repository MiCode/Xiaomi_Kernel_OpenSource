#
# Makefile for trusty components
#

subdir-ccflags-y += -Werror

obj-$(CONFIG_TRUSTY)		+= trusty.o
obj-$(CONFIG_TRUSTY)		+= trusty-irq.o
obj-$(CONFIG_TRUSTY_FIQ)	+= trusty-fiq.o
obj-$(CONFIG_TRUSTY_FIQ_ARM)	+= trusty-fiq-arm.o
obj-$(CONFIG_TRUSTY_FIQ_ARM64)	+= trusty-fiq-arm64.o trusty-fiq-arm64-glue.o
obj-$(CONFIG_TRUSTY_LOG)	+= trusty-log.o
obj-$(CONFIG_TRUSTY)		+= trusty-mem.o
obj-$(CONFIG_TRUSTY_VIRTIO)	+= trusty-virtio.o
obj-$(CONFIG_TRUSTY_VIRTIO_IPC)	+= trusty-ipc.o
obj-$(CONFIG_HAVE_MTK_IN_HOUSE_TEE_SUPPORT)	+= trusty-mtee.o
