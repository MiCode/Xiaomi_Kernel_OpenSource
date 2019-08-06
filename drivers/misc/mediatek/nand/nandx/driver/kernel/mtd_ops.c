/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#include <linux/mtd/mtd.h>
#include "nandx_ops.h"
#include "nandx_core.h"

int nand_write(struct mtd_info *mtd, loff_t to, size_t len,
	       size_t *retlen, const uint8_t *buf)
{
	int ret;
	struct nandx_core *ncore;

	ncore = (struct nandx_core *)mtd->priv;
	ret = nandx_ops_write(ncore, to, len, (u8 *)buf, DO_SINGLE_PLANE_OPS);
	if (ret < 0)
		return ret;

	*retlen = len;
	return 0;
}

int nand_read(struct mtd_info *mtd, loff_t from, size_t len,
	      size_t *retlen, uint8_t *buf)
{
	int ret;
	struct nandx_core *ncore;

	ncore = (struct nandx_core *)mtd->priv;
	ret = nandx_ops_read(ncore, from, len, buf, DO_SINGLE_PLANE_OPS);
	if (ret < 0)
		return ret;

	*retlen = len;
	return 0;
}

int nand_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct nandx_core *ncore;
	int ret;

	ncore = (struct nandx_core *)mtd->priv;

	instr->state = MTD_ERASING;
	ret = nandx_ops_erase(ncore, instr->addr, 0, instr->len);
	instr->state = ret < 0 ? MTD_ERASE_FAILED : MTD_ERASE_DONE;
	ret = ret < 0 ? -EIO : 0;
	if (!ret)
		mtd_erase_callback(instr);

	return ret;
}

int nand_read_oob(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops)
{
	struct nandx_core *ncore;

	ncore = (struct nandx_core *)mtd->priv;
	return nandx_ops_read_oob(ncore, from, ops->oobbuf);
}

int nand_write_oob(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops)
{
	struct nandx_core *ncore;

	ncore = (struct nandx_core *)mtd->priv;
	return nandx_ops_read_oob(ncore, to, ops->oobbuf);
}

#ifdef CONFIG_MTK_TLC_NAND_SUPPORT
int nand_mark_bad(struct mtd_info *mtd, loff_t ofs, const uint8_t *buf)
#else
int nand_mark_bad(struct mtd_info *mtd, loff_t ofs)
#endif
{
	return 0;
}

int nand_is_bad(struct mtd_info *mtd, loff_t ofs)
{
	return 0;
}

void nand_sync(struct mtd_info *mtd)
{

}
