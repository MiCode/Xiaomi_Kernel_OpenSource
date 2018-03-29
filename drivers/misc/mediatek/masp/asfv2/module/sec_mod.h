/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef SECMOD_H
#define SECMOD_H

#include <linux/init.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/fs.h>

struct sec_ops {
	int (*sec_get_rid)(unsigned int *rid);
};

struct sec_mod {
	dev_t id;
	int init;
	spinlock_t lock;
	const struct sec_ops *ops;
};

/**************************************************************************
 *  EXTERNAL VARIABLE
 **************************************************************************/
extern const struct sec_ops *sec_get_ops(void);
extern struct semaphore hacc_sem;
/**************************************************************************
 *  EXTERNAL FUNCTION
 **************************************************************************/
extern long sec_core_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
extern void sec_core_init(void);
extern void sec_core_exit(void);
#define NUM_SBC_PUBK_HASH           8
#define NUM_CRYPTO_SEED          16
#define NUM_RID 4


#ifdef CONFIG_OF
/*device information data*/
struct masp_tag {
	u32 size;
	u32 tag;
	unsigned int rom_info_sbc_attr;
	unsigned int rom_info_sdl_attr;
	unsigned int hw_sbcen;
	unsigned int lock_state;
	unsigned int rid[NUM_RID];
	/*rom_info.m_SEC_KEY.crypto_seed */
	unsigned char crypto_seed[NUM_CRYPTO_SEED];
	unsigned int sbc_pubk_hash[NUM_SBC_PUBK_HASH];
};
#endif
#endif				/* end of SECMOD_H */
