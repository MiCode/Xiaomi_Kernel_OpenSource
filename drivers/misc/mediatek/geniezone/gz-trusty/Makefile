#
# Makefile for trusty components
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public

obj-$(CONFIG_MTK_ENABLE_GENIEZONE) += gz_trusty_mod.o
gz_trusty_mod-objs += trusty.o smcnr-table.o
ifeq ($(CONFIG_MT_GZ_TRUSTY_DEBUGFS),y)
gz_trusty_mod-objs += trusty-debugfs.o
endif

obj-$(CONFIG_MTK_GZ_IRQ) += gz_irq_mod.o
gz_irq_mod-objs += trusty-irq.o smcnr-table.o

obj-$(CONFIG_MTK_GZ_VIRTIO) += gz_virtio_mod.o
gz_virtio_mod-objs += trusty-virtio.o trusty-mem.o smcnr-table.o

obj-$(CONFIG_MTK_GZ_VIRTIO_IPC) += gz_ipc_mod.o
gz_ipc_mod-objs += trusty-ipc.o

obj-$(CONFIG_MTK_GZ_LOG) += gz_log_mod.o
gz_log_mod-objs += gz-log.o smcnr-table.o

