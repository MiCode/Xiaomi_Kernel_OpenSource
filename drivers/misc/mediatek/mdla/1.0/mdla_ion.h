/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
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

