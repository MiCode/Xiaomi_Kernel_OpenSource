# SPDX-License-Identifier: GPL-2.0
#
# Makefile for mi memory info
#

obj-$(CONFIG_MI_MEMORY_SYSFS) 	+= mi_memory.o

mi_memory-$(CONFIG_MI_MEMORY_SYSFS) += mi_memory_sysfs.o
mi_memory-$(CONFIG_MI_MEMORY_SYSFS) += mi_ufs_info.o
mi_memory-$(CONFIG_MI_MEMORY_SYSFS) += mi_dram_info.o
mi_memory-$(CONFIG_MI_MEMORY_SYSFS) += mem_interface.o