/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * Test bad block mark on MTD device.
 *
 * Author: Xiaolei Li <xiaolei.li@mediatek.com>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <asm/div64.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/random.h>
#include <linux/slab.h>

#include "mtd_test.h"

static int dev = -EINVAL;
module_param(dev, int, 0444);
MODULE_PARM_DESC(dev, "MTD device number to use");

static int mode;
module_param(mode, int, 0444);
MODULE_PARM_DESC(mode, "0=just show bad block info, 1=mark bad block test");

static int mark_block;
module_param(mark_block, int, 0444);
MODULE_PARM_DESC(mark_block, "the block which will be marked as bad");

static struct mtd_info *mtd;
static unsigned char *bbt;
static int ebcnt;

static int rand_eb(void)
{
	unsigned int eb;

again:
	eb = prandom_u32();
	/* Read or write up 2 eraseblocks at a time - hence 'ebcnt - 1' */
	eb %= (ebcnt - 1);
	if (bbt[eb])
		goto again;
	return eb;
}

static int __init mtd_markbadtest_init(void)
{
	int err = 0;
	uint64_t tmp;

	pr_info("\n=================================================\n");

	if (dev < 0) {
		pr_info("Please specify a valid mtd-device\n");
		return -EINVAL;
	}

	pr_info("MTD device: %d\n", dev);

	mtd = get_mtd_device(NULL, dev);
	if (IS_ERR(mtd)) {
		err = PTR_ERR(mtd);
		pr_info("error: cannot get MTD device\n");
		return err;
	}

	if (!mtd_type_is_nand(mtd)) {
		pr_info("this test requires NAND flash\n");
		goto out;
	}

	tmp = mtd->size;
	do_div(tmp, mtd->erasesize);
	ebcnt = tmp;

	pr_info("MTD device size %llu, eraseblock size %u, count of eraseblocks %u\n",
		(unsigned long long)mtd->size, mtd->erasesize, ebcnt);

	err = -ENOMEM;

	bbt = kzalloc(ebcnt, GFP_KERNEL);
	if (!bbt)
		goto out;
	err = mtdtest_scan_for_bad_eraseblocks(mtd, bbt, 0, ebcnt);
	if (err)
		goto out;

	if (mode) {
		if (!mark_block)
			mark_block = rand_eb();
		else {
			if (mark_block < 0 || mark_block >= ebcnt) {
				err = -EINVAL;
				goto out;
			}
			if (bbt[mark_block]) {
				err = -EIO;
				goto out;
			}
		}
		pr_info("Mark block %d as bad block\n", mark_block);
		err = mtd_block_markbad(mtd,
					(loff_t)mark_block * mtd->erasesize);
		if (err) {
			pr_info("failed to mark block %d as bad block\n",
				mark_block);
			goto out;
		}

		if (mtd_block_isbad(mtd, (loff_t)mark_block * mtd->erasesize))
			pr_info("Successfully mark bad block test\n");
		else
			pr_info("Failed mark bad block test\n");
	}
out:

	kfree(bbt);
	put_mtd_device(mtd);
	if (err)
		pr_info("error %d occurred\n", err);
	pr_info("=================================================\n");
	return err;
}
module_init(mtd_markbadtest_init);

static void __exit mtd_markbadtest_exit(void)
{
}
module_exit(mtd_markbadtest_exit);

MODULE_DESCRIPTION("bad block mark test");
MODULE_AUTHOR("Xiaolei Li");
MODULE_LICENSE("GPL");
