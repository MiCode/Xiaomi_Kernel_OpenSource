/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef	__OPS_H__
#define __OPS_H__

#include "nandx_util.h"
#include "nandx_core.h"

struct nandx_lock {
	void *lock;
	wait_queue_head_t wq;
	int state;
};

typedef int (*nandx_core_rw_cb) (struct nandx_ops *, int, u32);

int nandx_ops_read(struct nandx_core *dev, long long from,
		   size_t len, u8 *buf, bool do_multi);
int nandx_ops_write(struct nandx_core *dev, long long to,
		    size_t len, u8 *buf, bool do_multi);
int nandx_ops_read_oob(struct nandx_core *dev, long long to, u8 *oob);
int nandx_ops_write_oob(struct nandx_core *dev, long long to, u8 *oob);
int nandx_ops_erase_block(struct nandx_core *dev, long long laddr);
int nandx_ops_erase(struct nandx_core *dev, long long offs,
		    long long limit, size_t size);
int nandx_ops_mark_bad(u32 block, int reason);
int nandx_ops_isbad(long long offs);
u32 nandx_ops_addr_transfer(struct nandx_core *dev, long long laddr,
			    u32 *blk, u32 *map_blk);
u32 nandx_get_chip_block_num(struct nandx_chip_info *info);
u32 nandx_calculate_bmt_num(struct nandx_chip_info *info);
void dump_nand_info(struct nandx_chip_info *info);
bool randomizer_is_support(enum IC_VER ver);
struct nandx_core *nandx_device_init(u32 mode);
int nandx_get_device(int new_state);
void nandx_release_device(void);
void nandx_lock_init(void);
struct nandx_lock *get_nandx_lock(void);

#endif				/* __OPS_H__ */
