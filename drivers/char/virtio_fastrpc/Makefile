# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_VIRTIO_FASTRPC)    += vfastrpc.o

vfastrpc-objs := virtio_fastrpc_base.o virtio_fastrpc_core.o \
				virtio_fastrpc_mem.o virtio_fastrpc_queue.o
ifdef CONFIG_COMPAT
  vfastrpc-objs += ../adsprpc_compat.o
endif
