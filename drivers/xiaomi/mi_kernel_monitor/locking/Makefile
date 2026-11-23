# SPDX-License-Identifier: GPL-2.0+
# Copyright (C) 2024 Xiaomi Ltd. All rights reserved.

LINUXINCLUDE += -I$(srctree)/

obj-$(CONFIG_MI_LOCKING) += mi_locking.o

mi_locking-y += locking_main.o
mi_locking-y += holdlock.o
mi_locking-y += holdlock_proc.o
mi_locking-y += waitlock.o
mi_locking-y += waitlock_sort.o