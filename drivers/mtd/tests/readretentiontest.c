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
 * Test read retention on MTD device.
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
#include <linux/ktime.h>

#include "mtd_test.h"

static int dev = -EINVAL;
module_param(dev, int, 0444);
MODULE_PARM_DESC(dev, "MTD device number to use");

static int wpgcnt;
module_param(wpgcnt, int, 0444);
MODULE_PARM_DESC(mode, "number of pages to write");

static int rpgcnt;
module_param(rpgcnt, int, 0444);
MODULE_PARM_DESC(mode, "number of pages to read");

static int testblk;
module_param(testblk, int, 0444);
MODULE_PARM_DESC(testblk, "test block number within the MTD device");

static unsigned int cycles;
module_param(cycles, uint, 0444);
MODULE_PARM_DESC(cycles, "how much cycles to read");

static int gran = 512;
module_param(gran, int, 0444);
MODULE_PARM_DESC(gran, "how often the status information should be printed");

static unsigned int pattern = 3;
module_param(pattern, uint, 0444);
MODULE_PARM_DESC(pattern, "1=5A5, 2=A5A, 3=random");

static struct mtd_info *mtd;
static unsigned char *bbt;
static unsigned char *patt_buf;
/* This is used to check data */
static unsigned char *check_buf;

static int ebcnt;
static struct rnd_state rnd_state;
static ktime_t start, finish;

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

static int __init mtd_readretentiontest_init(void)
{
	int err = 0, ppb, i, infinite = !cycles;
	uint64_t tmp;
	unsigned int read_cycle = 0;
	struct mtd_ecc_stats oldstats;

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

	if (!testblk)
		testblk = rand_eb();
	else {
		if (testblk < 0 || testblk >= ebcnt) {
			err = -EINVAL;
			goto out;
		}
		if (bbt[testblk]) {
			err = -EIO;
			goto out;
		}
	}

	ppb = mtd->erasesize / mtd->writesize;
	if (!wpgcnt)
		wpgcnt = ppb;
	if (!rpgcnt)
		rpgcnt = ppb;
	pr_info("read retention test block: %d, consecutive write pages: %d, consecutinve read pages: %d\n",
		testblk, wpgcnt, rpgcnt);

	err = -ENOMEM;
	check_buf = kmalloc(mtd->erasesize, GFP_KERNEL);
	if (!check_buf)
		goto out;

	patt_buf = kmalloc(mtd->erasesize, GFP_KERNEL);
	if (!patt_buf)
		goto out_checkbuf;

	/* paterrn 5A5 test */
	if (pattern == 1) {
		pr_info("5A5 pattern\n");
		for (i = 0; i < ppb; i++) {
			if (!(i & 1))
				memset(patt_buf + i * mtd->writesize, 0x55,
					mtd->writesize);
			else
				memset(patt_buf + i * mtd->writesize, 0xAA,
					mtd->writesize);
		}
	} else if (pattern == 2) {
		/* pattern A5A test */
		pr_info("A5A pattern\n");
		for (i = 0; i < ppb; i++) {
			if (!(i & 1))
				memset(patt_buf + i * mtd->writesize, 0xAA,
					mtd->writesize);
			else
				memset(patt_buf + i * mtd->writesize, 0x55,
					mtd->writesize);
		}
	} else if (pattern == 3) {
		/* pattern random test */
		pr_info("random pattern\n");
		prandom_seed_state(&rnd_state, 1);
		prandom_bytes_state(&rnd_state, patt_buf, mtd->erasesize);
	}

	err = mtdtest_erase_eraseblock(mtd, testblk);
	if (err)
		goto out_pattbuf;

	err = mtdtest_write(mtd, (loff_t)testblk * mtd->erasesize,
				(size_t)wpgcnt * mtd->writesize, patt_buf);
	if (err)
		goto out_pattbuf;

	start = ktime_get();
	memcpy(&oldstats, &mtd->ecc_stats, sizeof(oldstats));

	while (1) {
		err = mtdtest_read(mtd, (loff_t)testblk * mtd->erasesize,
				(size_t)rpgcnt * mtd->writesize, check_buf);
		if (err)
			goto out_pattbuf;

		if (memcmp(patt_buf, check_buf,
				(size_t)rpgcnt * mtd->writesize)) {
			pr_info("read check fail\n");
			err = -EBADMSG;
			goto out_pattbuf;
		}

		read_cycle++;

		if (read_cycle % gran == 0) {
			long ms;

			finish = ktime_get();
			ms = ktime_ms_delta(finish, start);

			err = mtd->ecc_stats.corrected - oldstats.corrected;

			pr_info("%u read cycles done, took %lu, milliseconds, err corrected %d\n",
				read_cycle, ms, err);
			err = mtdtest_relax();
			if (err)
				goto out_pattbuf;
			start = ktime_get();
			memcpy(&oldstats, &mtd->ecc_stats, sizeof(oldstats));
		}

		if (!infinite && --cycles == 0)
			break;
	}

out_pattbuf:
	pr_info("finished after %u read cycles\n", read_cycle);
	kfree(patt_buf);
out_checkbuf:
	kfree(check_buf);
out:

	kfree(bbt);
	put_mtd_device(mtd);
	if (err)
		pr_info("error %d occurred\n", err);
	pr_info("=================================================\n");
	return err;
}
module_init(mtd_readretentiontest_init);

static void __exit mtd_readretentiontest_exit(void)
{
}
module_exit(mtd_readretentiontest_exit);

MODULE_DESCRIPTION("read retention test");
MODULE_AUTHOR("Xiaolei Li");
MODULE_LICENSE("GPL");
