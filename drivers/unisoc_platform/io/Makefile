# SPDX-License-Identifier: GPL-2.0-only

KCOV_INSTRUMENT := n
KCSAN_SANITIZE := n

obj-$(CONFIG_UNISOC_IODEBUG) += kprobe_block.o
obj-$(CONFIG_UNISOC_IOWAIT) += kprobe_iowait.o
obj-$(CONFIG_UNISOC_IOLIMIT) += iolimit.o
obj-$(CONFIG_UNISOC_FILEMAP_DEBUG) += kprobe_filemap.o

obj-$(CONFIG_UNISOC_IOMONITOR) += kprobe_iomonitor.o

kprobe_iomonitor-y := \
        iomonitor.o \
        qsort.o
