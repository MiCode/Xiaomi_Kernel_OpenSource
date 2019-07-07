#
# Makefile for trusty components
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public

obj-$(CONFIG_MTK_ENABLE_GENIEZONE)	+= trusty.o
obj-$(CONFIG_MTK_ENABLE_GENIEZONE)	+= trusty-irq.o
obj-$(CONFIG_TRUSTY_FIQ)		+= trusty-fiq.o
obj-$(CONFIG_TRUSTY_FIQ_ARM)		+= trusty-fiq-arm.o
obj-$(CONFIG_TRUSTY_FIQ_ARM64)		+= trusty-fiq-arm64.o trusty-fiq-arm64-glue.o
ifeq ($(CONFIG_MTK_ENABLE_GENIEZONE),y)
obj-$(CONFIG_TRUSTY_LOG)		+= gz-log.o
obj-$(CONFIG_MTK_ENABLE_GENIEZONE)	+= trusty-nebula.o
ifeq ($(CONFIG_MTK_NEBULA_VM_SUPPORT),y)
obj-$(CONFIG_MTK_ENABLE_GENIEZONE)	+= trusty-nebula.o
endif
endif
obj-$(CONFIG_MTK_ENABLE_GENIEZONE)	+= trusty-mem.o
obj-$(CONFIG_TRUSTY_VIRTIO)		+= trusty-virtio.o
obj-$(CONFIG_TRUSTY_VIRTIO_IPC)	+= trusty-ipc.o


