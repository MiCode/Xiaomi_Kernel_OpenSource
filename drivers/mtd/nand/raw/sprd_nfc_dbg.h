/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unisoc NAND driver
 *
 * Copyright (C) 2023 Unisoc, Inc.
 * Author: Zhenxiong Lai <zhenxiong.lai@unisoc.com>
 */
#ifndef _SPRD_NFC_DBG_H_
#define _SPRD_NFC_DBG_H_

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mtd/nand.h>

struct sprd_nfc_dbg_dump {
	bool valid;
	ktime_t time;
	pid_t pid;
	u8 cpu;

	u32 reg[160];
};

struct sprd_nfc_dbg_inst {
	u32 start;
	u32 cfg0;
	u32 cfg4;
	u32 tim0;
	u32 sts_match;

	u32 inst0[24];

	u32 intr;
	u32 status[8];
};

struct sprd_snfc_dbg_inst {
	u32 start;
	u32 cfg0;
	u32 cfg4;
	u32 tim0;
	u32 sts_match;
	u32 com_cfg;

	u32 inst0[24];

	u32 intr;
	u32 status;
	u32 get_features;
	u32 sub_inst0;
};

#define SPRD_DBG_MAGIC_SNFC	0x21697073
#define SPRD_DBG_MAGIC_NFC	0x21776172
struct sprd_nfc_dbg_exec {
	u32 magic;
	bool valid;
	ktime_t time_s;
	ktime_t time_e;
	pid_t pid;
	u32 cpu;

	union {
		struct sprd_nfc_dbg_inst nfc;
		struct sprd_snfc_dbg_inst snfc;
	} d;
};

#define SPRD_NFC_EXEC_DBG_MAX	128
struct sprd_nfc_dbg {
	void *priv;
	struct sprd_nfc_dbg_dump startup;
	struct sprd_nfc_dbg_dump last_err;

	int dbg_idx;
	struct sprd_nfc_dbg_exec exec[SPRD_NFC_EXEC_DBG_MAX];
};

void sprd_nfc_dbg_sav_startup(void *priv);
void sprd_nfc_dbg_sav_last_err(void *priv);
void sprd_nfc_dbg_sav_exec(void *priv, bool post);
int sprd_nfc_dbg_init(void *priv);
#endif

