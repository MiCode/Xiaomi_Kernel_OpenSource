/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#include "nandx_util.h"
#include "nandx_errno.h"
#include "nandx_info.h"
#include "nandx_bmt.h"
#include "nandx_pmt.h"
#include "nandx_core.h"
#include "nandx_platform.h"
#include "nandx_ops.h"

/*
 * this file is just for pl & lk & mtd,
 * mntl wrapper is independent
 */

struct ops_data {
	long long base;
	size_t len;
	u8 *data;
	u8 *oob;
};

struct wrap_ops {
	long long offs;
	size_t len;
	u8 *data;
	u8 *oob;
};

static struct nandx_lock nlock;

static int set_ops_table(struct nandx_chip_info *info,
			 struct nandx_ops *ops_table, int count,
			 struct ops_data *odata)
{
	u32 start_row;
	u32 page_size;
	size_t tmp_len, tmp_count;
	long long page_align_start;
	struct nandx_ops *ops, *tmp_ops;

	tmp_len = odata->len;
	ops = ops_table;
	tmp_ops = ops;

	page_size = info->page_size;

	page_align_start = div_round_down(odata->base, page_size);
	start_row = (u32)div_down(page_align_start, page_size);
	tmp_count = count;
	while (tmp_len) {
		ops->row = start_row;
		if (ops == ops_table) {
			ops->col = odata->base - page_align_start;
			ops->len = MIN(page_size - ops->col, tmp_len);
			ops->data = odata->data;
			ops->oob = odata->oob;
		} else {
			ops->col = 0;
			ops->len = MIN(page_size, tmp_len);
			ops->data = tmp_ops->data + tmp_ops->len;
			/* only one oob data */
			ops->oob = odata->oob;
		}

		tmp_ops = ops;
		tmp_len -= ops->len;
		start_row++;
		ops++;
		tmp_count--;
	}

	NANDX_ASSERT(tmp_count == 0);
	return 0;
}

static struct nandx_ops *get_error_ops(struct nandx_ops *ops_table, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (ops_table[i].status < 0 &&
		    ops_table[i].status != -ENANDFLIPS)
			return &ops_table[i];
	}

	return NULL;
}

static int nandx_ops_block_rw(struct nandx_core *ncore,
			      struct wrap_ops *wrapops, bool is_raw,
			      bool do_multi, void *callback,
			      struct nandx_ops *err_ops)
{
	size_t new_len;
	int ret, ops_count, plane_num, mode = 0;
	long long page_align_start, end;
	u32 page_size;
	struct ops_data odata;
	struct nandx_ops *ops, *ops_table;
	struct nandx_chip_info *info;
	nandx_core_rw_cb nandx_core_rw;

	info = ncore->info;
	plane_num = info->plane_num;
	if (plane_num > 1 && do_multi)
		mode |= MODE_MULTI;

	if (!do_multi)
		plane_num = 1;

	if (is_raw)
		mode |= MODE_SLC;

	page_size = info->page_size;

	end = wrapops->offs + wrapops->len - 1;
	page_align_start = div_round_down(wrapops->offs, page_size);
	new_len = end - page_align_start + 1;

	ops_count = div_up(new_len, page_size);
	if (ops_count % plane_num) {
		pr_err("%s: count %d not plane align\n", __func__, ops_count);
		return -EINVAL;
	}

	ops_table = alloc_ops_table(ops_count);
	if (!ops_table) {
		NANDX_ASSERT(ops_table);
		return -ENOMEM;
	}

	odata.base = wrapops->offs;
	odata.len = wrapops->len;
	odata.data = wrapops->data;
	odata.oob = wrapops->oob;
	ret = set_ops_table(info, ops_table, ops_count, &odata);
	if (ret < 0)
		goto err;

	if (ops_count / plane_num > 1)
		mode |= MODE_CACHE;

	nandx_core_rw = (nandx_core_rw_cb)callback;
	ret = nandx_core_rw(ops_table, ops_count, mode);
	if (ret < 0 && ret != -ENANDFLIPS) {
		if (err_ops) {
			ops = get_error_ops(ops_table, ops_count);
			NANDX_ASSERT(ops);
			if (ops)
				memcpy(err_ops, ops,
				       sizeof(struct nandx_ops));
		}
		goto err;
	}

err:
	free_ops_table(ops_table);
	return ret;
}

static int nandx_write_error_handler(struct nandx_core *ncore,
				     u32 err_blk, u32 err_page,
				     u8 *buf, bool do_multi)
{
	u8 *data;
	bool raw_part;
	size_t size;
	long long err_addr, new_addr;
	u32 new_block, oob_size, page_size, block_size;
	int ret, plane_num;
	struct wrap_ops wrap;
	struct pt_resident *pt;

	page_size = ncore->info->page_size;
	block_size = ncore->info->block_size;
	oob_size = ncore->info->oob_size;
	err_addr = err_blk * block_size;
	pt = nandx_pmt_get_partition(err_addr);
	raw_part = nandx_pmt_is_raw_partition(pt);

	plane_num = ncore->info->plane_num;
	if (!do_multi)
		plane_num = 1;

	if (raw_part) {
		data = mem_alloc(err_page, page_size);
		if (!data) {
			NANDX_ASSERT(buf);
			return -ENOMEM;
		}
		size = err_page * page_size;
	} else {
		data = buf;
		size = block_size * plane_num;
	}

	wrap.offs = err_addr;
	wrap.len = size;
	wrap.data = data;
	wrap.oob = mem_alloc(1, oob_size);
	if (!wrap.oob) {
		ret = -ENOMEM;
		goto read_err;
	}

	nandx_bmt_update_oob(err_blk, wrap.oob);
	ret = nandx_ops_block_rw(ncore, &wrap, raw_part,
				 do_multi, nandx_core_read, NULL);
	if (ret < 0)
		goto oob_err;
	new_block = nandx_ops_mark_bad(err_blk, UPDATE_WRITE_FAIL);
	new_addr = new_block * block_size;
	wrap.offs = new_addr;
	ret = nandx_ops_block_rw(ncore, &wrap, raw_part,
				 do_multi, nandx_core_write, NULL);

oob_err:
	mem_free(wrap.oob);
read_err:
	if (raw_part)
		mem_free(data);
	return ret;
}

static int nandx_ops_rw(struct nandx_core *ncore,
			struct wrap_ops *wrapops,
			bool do_multi, void *callback)
{
	u8 *data;
	bool raw_part;
	size_t size, left;
	int ret, block_page_num;
	long long addr, map_addr;
	u32 page_size, oob_size, erase_size;
	u32 err_page, block, map_block;
	struct nandx_ops err_ops;
	struct wrap_ops wrap;
	struct pt_resident *pt;
	struct nandx_chip_info *info;

	info = ncore->info;
	page_size = info->page_size;
	oob_size = info->oob_size;
	erase_size = info->block_size;
	block_page_num = info->block_size / page_size;

	addr = wrapops->offs;
	left = wrapops->len;
	data = wrapops->data;

	pt = nandx_pmt_get_partition(addr);
	raw_part = nandx_pmt_is_raw_partition(pt);
	if (raw_part)
		erase_size /= info->wl_page_num;
	size = erase_size - reminder(wrapops->offs, erase_size);
	size = MIN(wrapops->len, size);

	wrap.oob = mem_alloc(1, oob_size);
	if (!wrap.oob)
		return -ENOMEM;

	while (left) {
		/*
		 * unmark it if want to support cross partitions r/w.
		 * pt = nandx_pmt_get_partition(addr);
		 * raw_part = nandx_pmt_is_raw_partition(pt);
		 * erase_size = info->block_size;
		 * if (raw_part)
		 *      erase_size = info->block_size / info->wl_page_num;
		 */
		nandx_ops_addr_transfer(ncore, addr, &block, &map_block);
		map_addr = (long long)map_block * info->block_size +
		    reminder(addr, erase_size);

		wrap.offs = map_addr;
		wrap.len = size;
		wrap.data = data;
		nandx_bmt_update_oob(block, wrap.oob);

		ret = nandx_ops_block_rw(ncore, &wrap, raw_part,
					 do_multi, callback, &err_ops);
		if (ret < 0 && ret != -ENANDFLIPS) {
			if (err_ops.status != -ENANDWRITE)
				goto oob_err;
			err_page = err_ops.row % block_page_num;
			ret = nandx_write_error_handler(ncore, map_block,
							err_page, data,
							do_multi);
			if (ret < 0)
				goto oob_err;
		}
		addr += size;
		data += size;
		left -= size;
		size = MIN(left, erase_size);
	}

oob_err:
	mem_free(wrap.oob);
	return 0;
}

static int nandx_ops_rw_oob(struct nandx_core *ncore, long long offs, u8 *oob,
			    void *callback)
{
	bool raw_part;
	int ret, mode = 0;
	u32 page_size;
	struct nandx_ops ops;
	struct nandx_chip_info *info;
	nandx_core_rw_cb nandx_core_rw;
	struct pt_resident *pt;

	info = ncore->info;
	if (reminder(offs, info->page_size)) {
		pr_err("%s: addr %llu not sector align\n", __func__, offs);
		return -EINVAL;
	}

	pt = nandx_pmt_get_partition(offs);
	raw_part = nandx_pmt_is_raw_partition(pt);

	if (raw_part)
		mode |= MODE_SLC;

	page_size = info->page_size;

	ops.row = (u32)div_down(offs, page_size);
	ops.col = 0;
	ops.len = 0;
	ops.data = NULL;
	ops.oob = oob;
	nandx_core_rw = (nandx_core_rw_cb)callback;
	ret = nandx_core_rw(&ops, 1, mode);
	if (ret < 0)
		goto err;

	return 0;

err:
	return ret;
}

int nandx_ops_read(struct nandx_core *ncore, long long from,
		   size_t len, u8 *buf, bool do_multi)
{
	struct wrap_ops wrap;
	int ret;

	nandx_get_device(FL_READING);

	wrap.offs = from;
	wrap.len = len;
	wrap.data = buf;
	ret = nandx_ops_rw(ncore, &wrap, do_multi, nandx_core_read);

	nandx_release_device();
	return ret;
}

int nandx_ops_write(struct nandx_core *ncore, long long to,
		    size_t len, u8 *buf, bool do_multi)
{
	struct wrap_ops wrap;
	int ret;

	nandx_get_device(FL_WRITING);

	wrap.offs = to;
	wrap.len = len;
	wrap.data = buf;
	ret = nandx_ops_rw(ncore, &wrap, do_multi, nandx_core_write);

	nandx_release_device();
	return ret;
}

int nandx_ops_read_oob(struct nandx_core *ncore, long long from, u8 *oob)
{
	int ret;

	nandx_get_device(FL_READING);

	ret = nandx_ops_rw_oob(ncore, from, oob, nandx_core_read);

	nandx_release_device();
	return ret;
}

int nandx_ops_write_oob(struct nandx_core *ncore, long long to, u8 *oob)
{
	int ret;

	nandx_get_device(FL_WRITING);

	ret = nandx_ops_rw_oob(ncore, to, oob, nandx_core_write);

	nandx_release_device();
	return ret;
}

int nandx_ops_erase_block(struct nandx_core *ncore, long long laddr)
{
	bool raw_part;
	int plane_num, ret = 0;
	u32 block, map_block, mode = 0;
	u32 rows[2], page_size, block_size, block_page_num;
	struct nandx_chip_info *info;
	struct pt_resident *pt;

	nandx_get_device(FL_ERASING);

	info = ncore->info;
	page_size = info->page_size;
	block_size = info->block_size;
	block_page_num = block_size / page_size;
	plane_num = info->plane_num;

	pt = nandx_pmt_get_partition(laddr);
	raw_part = nandx_pmt_is_raw_partition(pt);
	if (raw_part) {
		mode |= MODE_SLC;
		plane_num = 1;
	} else {
		mode |= MODE_MULTI;
		/* Todo: check how to erase non-raw partition */
		NANDX_ASSERT(0);
	}

	rows[0] = nandx_ops_addr_transfer(ncore, laddr, &block, &map_block);

	/*
	 * it is not correct for multi plane,
	 * but we need not to use multi operation now.
	 */
	if (plane_num > 1)
		rows[1] = rows[0] + block_page_num;

	ret = nandx_core_erase(rows, plane_num, mode);
	if (ret < 0) {
		pr_err("erase block 0x%x failed\n", map_block);
		ret = nandx_ops_mark_bad(map_block, UPDATE_ERASE_FAIL);
	}

	nandx_release_device();
	return ret;
}

int nandx_ops_erase(struct nandx_core *ncore, long long offs,
		    long long limit, size_t size)
{
	long long cur_offs;
	u32 erase_size, block_cnt;
	struct nandx_chip_info *info;
	struct pt_resident *pt;
	bool raw_part;

	info = ncore->info;
	erase_size = info->block_size;

	pt = nandx_pmt_get_partition(offs);
	raw_part = nandx_pmt_is_raw_partition(pt);
	if (raw_part)
		erase_size /= info->wl_page_num;

	if (reminder(offs, erase_size) || reminder(size, erase_size)) {
		pr_debug("%s: invalid offs 0x%llx 0x%zx\n",
			 __func__, offs, size);
		return -EINVAL;
	}

	/* calculate block number of this image */
	block_cnt = size / erase_size;

	/* erase nand block */
	cur_offs = offs;
	while (block_cnt) {
		if (nandx_ops_erase_block(ncore, cur_offs) < 0) {
			pr_warn("erase 0x%llx fail\n", cur_offs);
			nandx_ops_mark_bad((u32)
					   div_down(cur_offs, erase_size),
					   UPDATE_ERASE_FAIL);
		}

		cur_offs += erase_size;
		block_cnt--;

		if (limit && block_cnt && cur_offs >= limit) {
			pr_warn("off 0x%llx exceed 0x%llx\n",
				cur_offs, limit);
			return 0;
		}
	}

	return 0;
}

int nandx_ops_mark_bad(u32 block, int reason)
{
	return nandx_bmt_update(block, reason);
}

int nandx_ops_isbad(long long offs)
{
	return 0;
}

u32 nandx_ops_addr_transfer(struct nandx_core *ncore, long long laddr,
			    u32 *blk, u32 *map_blk)
{
	int block_page_num;
	u32 page_size, block_size;
	u32 page_in_block, block;

	page_size = ncore->info->page_size;
	block_size = ncore->info->block_size;
	block_page_num = block_size / page_size;

	nandx_pmt_addr_to_row(laddr, &block, &page_in_block);
	*blk = block;
	*map_blk = nandx_bmt_get_mapped_block(block);
	return *map_blk * block_page_num + page_in_block;
}

void dump_nand_info(struct nandx_chip_info *info)
{
	pr_info("block_num is %u\n", info->block_num);
	pr_info("block_size is %u\n", info->block_size);
	pr_info("page_size is %u\n", info->page_size);
	pr_info("slc_block_size is %u\n", info->slc_block_size);
	pr_info("oob_size is %u\n", info->oob_size);
	pr_info("sector_size is %u\n", info->sector_size);
	pr_info("plane_num is %d\n", info->plane_num);
	pr_info("wl_page_num is %d\n", info->wl_page_num);
	pr_info("ecc_strength is %u\n", info->ecc_strength);
}

u32 nandx_calculate_bmt_num(struct nandx_chip_info *info)
{
	u32 ratio;

	if (is_support_mntl())
		ratio = info->block_num > 2000 ? 1 : 2;
	else
		ratio = 6;

	return (info->block_num / 100 * ratio);
}

u32 nandx_get_chip_block_num(struct nandx_chip_info *info)
{
	u32 chip_block_num, bmt_block_num;

	bmt_block_num = nandx_calculate_bmt_num(info);
	chip_block_num = info->block_num - bmt_block_num - PMT_POOL_SIZE;

	return chip_block_num;
}

struct nfc_resource nandx_resource[] = {
	/* NANDX_MT8127 */
	{NANDX_MT8127, (void *)NFI_BASE, (void *)NFIECC_BASE,
	 (void *)0x10005000, 0, 0, NULL},
	/* NANDX_MT8163 */
	{NANDX_MT8163, (void *)NFI_BASE, (void *)NFIECC_BASE,
	 (void *)0x10005000, 0, 0, NULL},
	/* NANDX_MT8167 */
	{NANDX_MT8167, (void *)NFI_BASE, (void *)NFIECC_BASE,
	 (void *)0x10005000, 0, 0, NULL},
	/* NANDX_NONE */
	{NANDX_NONE, 0, 0, 0, 0, 0, NULL},
};

struct nandx_core *nandx_device_init(u32 mode)
{
	int ret;
	u32 bmt_block_num, pmt_block_start;
	struct platform_data *pdata;
	struct nandx_core *ncore;
	struct nandx_chip_info *info;
	enum IC_VER ver;

	pdata = mem_alloc(1, sizeof(struct platform_data));
	if (!pdata)
		return NULL;

	ver = nandx_get_chip_version();
	pdata->res = nandx_get_nfc_resource(ver);

	pdata->freq.freq_async = 133000000;
	pdata->freq.sel_2x_idx = -1;
	pdata->freq.sel_ecc_idx = -1;

	ret = nandx_platform_init(pdata);
	if (ret < 0)
		goto pdata_err;

	if (randomizer_is_support(pdata->res->ver))
		mode |= MODE_RANDOMIZE;

	ncore = nandx_core_init(pdata, mode);
	if (!ncore)
		goto platform_exit;

	info = ncore->info;
	dump_nand_info(info);
	bmt_block_num = nandx_calculate_bmt_num(info);

	ret = nandx_bmt_init(ncore->info, bmt_block_num, true);
	if (ret < 0) {
		pr_err("Error: init bmt failed\n");
		NANDX_ASSERT(0);
		goto bmt_err;
	}

	pmt_block_start = ncore->info->block_num - bmt_block_num;
	pmt_block_start -= PMT_POOL_SIZE;
	ret = nandx_pmt_init(ncore->info, pmt_block_start);
	if (ret < 0) {
		pr_err("Error: init pmt failed\n");
		NANDX_ASSERT(0);
		goto pmt_err;
	}

	return ncore;

pmt_err:
	/* bmt_exit(); */
bmt_err:
	nandx_core_free();
platform_exit:
	nandx_platform_power_down(pdata);
pdata_err:
	mem_free(pdata);
	return NULL;
}

void nandx_lock_init(void)
{
	init_waitqueue_head(&nlock.wq);
	nlock.lock = nand_lock_create();
	nlock.state = FL_READY;
}

struct nandx_lock *get_nandx_lock(void)
{
	return &nlock;
}

int nandx_get_device(int new_state)
{
	struct nandx_lock *nlock = get_nandx_lock();
	struct nandx_core *ncore = get_nandx_core();
	void *lock;
	wait_queue_head_t *wq;
	DECLARE_WAITQUEUE(wait, current);
	bool high_speed_en, ecc_clk_en;

	lock = nlock->lock;
	wq = &nlock->wq;

retry:
	nand_lock(lock);

	if (nlock->state == FL_READY) {
		if (new_state != FL_READY && new_state != FL_PM_SUSPENDED) {
			high_speed_en = ncore->pdata->freq.sel_2x_idx >= 0;
			ecc_clk_en = ncore->pdata->freq.sel_ecc_idx >= 0;
			nandx_platform_enable_clock(ncore->pdata,
						    high_speed_en,
						    ecc_clk_en);
		}

		nlock->state = new_state;
		nand_unlock(lock);
		return 0;
	}

	set_current_state(TASK_UNINTERRUPTIBLE);
	add_wait_queue(wq, &wait);
	nand_unlock(lock);
	schedule();
	remove_wait_queue(wq, &wait);
	goto retry;
}

void nandx_release_device(void)
{
	struct nandx_lock *nlock = get_nandx_lock();
	struct nandx_core *ncore = get_nandx_core();
	void *lock;
	wait_queue_head_t *wq;
	bool high_speed_en, ecc_clk_en;

	lock = nlock->lock;
	wq = &nlock->wq;

	/* Release the controller and the chip */
	nand_lock(lock);
	high_speed_en = ncore->pdata->freq.sel_2x_idx >= 0;
	ecc_clk_en = ncore->pdata->freq.sel_ecc_idx >= 0;
	if (nlock->state != FL_READY && nlock->state != FL_PM_SUSPENDED)
		nandx_platform_disable_clock(ncore->pdata, high_speed_en,
					     ecc_clk_en);
	nlock->state = FL_READY;
	wake_up(wq);
	nand_unlock(lock);
}
