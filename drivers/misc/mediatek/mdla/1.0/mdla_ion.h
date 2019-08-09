/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>

#ifdef CONFIG_MTK_MDLA_ION
void mdla_ion_init(void);
void mdla_ion_exit(void);
int mdla_ion_kmap(unsigned long arg);
int mdla_ion_kunmap(unsigned long arg);
void mdla_ion_sync(u64 hndl, void *kva, u32 size);
#else
static inline
void mdla_ion_init(void)
{
}
static inline
void mdla_ion_exit(void)
{
}
static inline
int mdla_ion_kmap(unsigned long arg)
{
	return -EINVAL;
}
static inline
int mdla_ion_kunmap(unsigned long arg)
{
	return -EINVAL;
}
static inline
void mdla_ion_sync(u64 hndl, void *kva, u32 size)
{
}
#endif

