/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef __NANDX_CORE_H__
#define __NANDX_CORE_H__

#include "nandx_info.h"

/*
 * now we just use multi operation in mntl,
 * so set single plane in pl & lk & mtd.
 */
#define DO_SINGLE_PLANE_OPS false

struct nandx_chip_dev;
struct wl_order_program;
typedef int (*order_program_cb) (struct nandx_chip_dev *,
				 struct wl_order_program *,
				 struct nandx_ops *, int, bool, bool);

struct wl_order_program {
	u32 wl_num;
	u32 total_wl_num;
	struct nandx_ops **ops_list;
	order_program_cb order_program_func;
};

struct nandx_core {
	struct nandx_chip_info *info;
	struct nandx_chip_dev *chip;
	struct wl_order_program program;
	struct platform_data *pdata;
};

struct nandx_ops *alloc_ops_table(int count);
void free_ops_table(struct nandx_ops *ops_table);
int nandx_core_read(struct nandx_ops *ops_table, int count, u32 mode);
int nandx_core_write(struct nandx_ops *ops_table, int count, u32 mode);
int nandx_core_erase(u32 *rows, int count, u32 mode);
bool nandx_core_is_bad(u32 row);
int nandx_core_mark_bad(u32 row);

int nandx_core_suspend(void);
int nandx_core_resume(void);
struct nandx_core *nandx_core_init(struct platform_data *pdata, u32 mode);
int nandx_core_exit(void);
void nandx_core_free(void);
struct nandx_chip_info *get_chip_info(void);
struct nandx_core *get_nandx_core(void);

#endif				/* __NANDX_CORE_H__ */
