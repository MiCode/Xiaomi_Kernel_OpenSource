/* Copyright (c) 2014-2015, 2017-2020 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef DIAG_MEMORYDEVICE_H
#define DIAG_MEMORYDEVICE_H
#include "diagchar.h"

struct diag_buf_tbl_t {
	unsigned char *buf;
	int len;
	int ctx;
};

struct diag_md_info {
	int id;
	int ctx;
	int mempool;
	int num_tbl_entries;
	int md_info_inited;
	spinlock_t lock;
	struct diag_buf_tbl_t *tbl;
	struct diag_mux_ops *ops;
};

extern struct diag_md_info diag_md[NUM_DIAG_MD_DEV];

int diag_md_init(void);
int diag_md_mdm_init(void);
void diag_md_exit(void);
void diag_md_mdm_exit(void);
void diag_md_open_all(void);
void diag_md_close_all(void);
void diag_md_open_device(int id);
void diag_md_close_device(int id);
void diag_md_clear_tbl_entries(int id);
int diag_md_register(int id, int ctx, struct diag_mux_ops *ops);
int diag_md_close_peripheral(int id, uint8_t peripheral);
int diag_md_write(int id, unsigned char *buf, int len, int ctx);
int diag_md_copy_to_user(char __user *buf, int *pret, size_t buf_size,
			 struct diag_md_session_t *info);
#endif
