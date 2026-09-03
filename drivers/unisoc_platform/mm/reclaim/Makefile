# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_UNISOC_MM_RECLAIM) += unisoc_mm_reclaim.o
unisoc_mm_reclaim-objs := reclaim.o

ifeq ($(CONFIG_UNISOC_MM_SHRINKLRU),y)
unisoc_mm_reclaim-objs += kshrink_lruvecd.o
endif

ifeq ($(CONFIG_UNISOC_MM_SHRINKSLAB),y)
unisoc_mm_reclaim-objs += kshrink_slabd.o
endif

ifeq ($(CONFIG_UNISOC_MM_DIRECT_SWAPPINESS),y)
unisoc_mm_reclaim-objs += direct_swappiness.o
endif

ifeq ($(CONFIG_UNISOC_MM_SHRINK_ANON),y)
unisoc_mm_reclaim-objs += shrink_anon.o
unisoc_mm_reclaim-objs += shrink_anon_notifier.o
endif
