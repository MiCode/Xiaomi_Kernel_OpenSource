/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef __MTD_OPS_H__
#define __MTD_OPS_H__

#include <linux/mtd/mtd.h>

int nand_write(struct mtd_info *mtd, loff_t to, size_t len,
	       size_t *retlen, const uint8_t *buf);
int nand_read(struct mtd_info *mtd, loff_t from, size_t len,
	      size_t *retlen, uint8_t *buf);
int nand_erase(struct mtd_info *mtd, struct erase_info *instr);
int nand_read_oob(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops);
int nand_write_oob(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops);
int nand_is_bad(struct mtd_info *mtd, loff_t ofs);
#ifdef CONFIG_MTK_TLC_NAND_SUPPORT
int nand_mark_bad(struct mtd_info *mtd, loff_t ofs, const uint8_t *buf);
#else
int nand_mark_bad(struct mtd_info *mtd, loff_t ofs);
#endif
void nand_sync(struct mtd_info *mtd);

#endif				/* __MTD_OPS_H__ */
