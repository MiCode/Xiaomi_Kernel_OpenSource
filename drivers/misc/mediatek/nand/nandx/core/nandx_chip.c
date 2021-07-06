/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */

#include "nandx_errno.h"
#include "nandx_chip.h"
#include "nandx_chip_common.h"

#define NANDX_CHIP_VERSION	"nandx.18.08.22"
#ifdef CONFIG_NANDX_EXPRESS
struct nandx_chip *nandx_express_chip;
#endif

static int nandx_chip_get_order_cycle(struct nandx_chip *chip, u32 row)
{
	int i, empty, cycle, max_count, max_cycle;
	u32 wl, wl_page_num;
	struct nandx_device_info *dev_info = chip->dev_info;

	wl_page_num = chip->chip_dev.info.wl_page_num;
	wl = row / wl_page_num;
	max_cycle = wl_page_num * wl_page_num;
	max_count = dev_info->plane_num * wl_page_num;
	empty = max_count;

	for (i = 0; i < max_count; i++) {
		if (chip->orders[i].wl == wl) {
			cycle = chip->orders[i].cycle / wl_page_num;
			break;
		}
		if (chip->orders[i].cycle >= max_cycle ||
		    chip->orders[i].cycle == 0)
			empty = i;
	}

	if (i == max_count) {
		NANDX_ASSERT(empty < max_count);
		chip->orders[empty].wl = wl;
		chip->orders[empty].cycle = 1;
		return 0;
	}

	chip->orders[i].cycle++;
	return cycle;
}

static int nandx_chip_get_status(struct nandx_chip *chip,
				 int status, int count)
{
	int threshold, rr_count;
	struct nandx_device_info *dev_info = chip->dev_info;
	struct read_retry_ops *rr_ops = chip->rr_ops;

	if (status == -ETIMEDOUT)
		return -ENANDREAD;

	if (status <= 0)
		return status;

	threshold = dev_info->xlc_life.bitflips_threshold;
	rr_count = rr_ops->loop_count >> 1;
	if (chip->slc_mode) {
		threshold = dev_info->slc_life.bitflips_threshold;
		rr_count = rr_ops->slc_loop_count >> 1;
	}

	if (status >= threshold)
		return -ENANDFLIPS;

	if (count >= rr_count)
		return -ENANDFLIPS;

	return NAND_OK;
}

static int nandx_chip_read(struct nandx_chip_dev *chip_dev,
			   struct nandx_ops *ops)
{
	int ret;
	int num, rr_count, cali_count = 0;
	struct nandx_chip *chip;
	struct slc_mode_ops *slc_ops;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	slc_ops = chip->slc_ops;
	num = ops->len / chip->nfc->sector_size;

calibration:
	rr_count = 0;
	if (chip->slc_mode)
		slc_mode_entry(chip, true);

	if (chip->randomize)
		nandx_chip_enable_randomizer(chip, ops->row, false);

retry:
	if (rr_count > 0 && chip->slc_mode)
		slc_mode_entry(chip, false);

	nandx_chip_read_page(chip, ops->row);
	nandx_chip_random_output(chip, ops->row, ops->col);
	ret = nandx_chip_read_data(chip, num, ops->data, ops->oob);
	if (ret == -ENANDREAD) {
		ret = setup_read_retry(chip, rr_count);
		if (!ret) {
			rr_count++;
			goto retry;
		}
		pr_err("nandx-rr1 %d %d\n", rr_count, ret);
		setup_read_retry(chip, -1);
	} else if (rr_count > 0) {
		pr_info("nandx-rr1 %d %d\n", rr_count, ret);
		setup_read_retry(chip, -1);
	}

	if (chip->randomize)
		nandx_chip_disable_randomizer(chip);

	if (chip->slc_mode)
		slc_mode_exit(chip);

	if (chip->calibration && ret == -ENANDREAD) {
		pr_info("auto-k row %u count %d\n", ops->row, cali_count);
		ret = nandx_chip_auto_calibration(chip, cali_count);
		if (ret) {
			cali_count++;
			goto calibration;
		}

		ret = -ENANDREAD;
	}

	if (cali_count > 0)
		nandx_chip_auto_calibration(chip, -1);

	ops->status = nandx_chip_get_status(chip, ret, rr_count);
	return ops->status;
}

static int nandx_chip_cache_read(struct nandx_chip_dev *chip_dev,
				 struct nandx_ops **ops_list, int count)
{
	int ret = NAND_OK, status = NAND_OK;
	bool last_rr = false;
	int i, num, col, rr_count, cali_count = 0;
	u32 row;
	void *data, *oob;
	struct nandx_chip *chip;
	struct slc_mode_ops *slc_ops;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	slc_ops = chip->slc_ops;

calibration:

	if (chip->slc_mode)
		slc_mode_entry(chip, true);

	row = ops_list[0]->row;
	nandx_chip_read_page(chip, row);
	for (i = 1; i <= count; i++) {
		num = ops_list[i - 1]->len / chip->nfc->sector_size;

		if (chip->randomize) {
			row = ops_list[i - 1]->row;
			nandx_chip_enable_randomizer(chip, row, false);
		}

		if (i == count && !last_rr) {
			nandx_chip_cache_read_last_page(chip);
		} else if (i < count) {
			row = ops_list[i]->row;
			if (chip->slc_mode)
				slc_mode_entry(chip, false);

			nandx_chip_cache_read_page(chip, row);
		}

		row = ops_list[i - 1]->row;
		col = ops_list[i - 1]->col;
		nandx_chip_random_output(chip, row, col);

		data = ops_list[i - 1]->data;
		oob = ops_list[i - 1]->oob;
		ret = nandx_chip_read_data(chip, num, data, oob);
		if (ret == -ENANDREAD) {
			/* read read retry */
			nandx_chip_cache_read_last_page(chip);
			/* need to delay? or read status? */
			ret =
			    nandx_chip_read_retry(chip, ops_list[i - 1],
						  &rr_count);
			if (ret == -ENANDREAD)
				break;
			if (i == count)
				break;
			if (i == count - 1)
				last_rr = true;

			row = ops_list[i]->row;
			nandx_chip_read_page(chip, row);
		}
		ops_list[i - 1]->status =
		    nandx_chip_get_status(chip, ret, rr_count);
		status = MIN(ops_list[i - 1]->status, status);
	}

	if (chip->randomize)
		nandx_chip_disable_randomizer(chip);

	if (chip->slc_mode)
		slc_mode_exit(chip);

	if (ret == -ENANDREAD) {
		ret = nandx_chip_auto_calibration(chip, cali_count);
		if (ret) {
			cali_count++;
			goto calibration;
		}

		ret = -ENANDREAD;
	}

	if (cali_count > 0)
		nandx_chip_auto_calibration(chip, -1);

	return status;
}

static int nandx_chip_multi_read(struct nandx_chip_dev *chip_dev,
				 struct nandx_ops **ops_list, int count)
{
	int ret = NAND_OK, col, status = NAND_OK;
	int i, j, num, rr_count, cali_count = 0;
	u32 row;
	void *data, *oob;
	struct nandx_chip *chip;
	struct slc_mode_ops *slc_ops;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	slc_ops = chip->slc_ops;

calibration:
	pr_debug("%s: count %d, slc_mode %d\n",
		 __func__, count, chip->slc_mode);
	if (chip->slc_mode)
		slc_mode_entry(chip, true);

	for (i = 0; i < count; i += 2) {
		row = ops_list[i]->row;
		nandx_chip_multi_read_page(chip, row);
		row = ops_list[i + 1]->row;

		if (chip->slc_mode && i > 0)
			slc_mode_entry(chip, false);

		nandx_chip_read_page(chip, row);

		for (j = 0; j < 2; j++) {
			num = ops_list[i + j]->len / chip->nfc->sector_size;

			row = ops_list[i + j]->row;
			col = ops_list[i + j]->col;
			if (chip->randomize)
				nandx_chip_enable_randomizer(chip, row,
							     false);

			nandx_chip_random_output(chip, row, col);

			data = ops_list[i + j]->data;
			oob = ops_list[i + j]->oob;
			ret = nandx_chip_read_data(chip, num, data, oob);
			if (ret == -ENANDREAD)
				ret = nandx_chip_read_retry(chip,
							    ops_list[i + j],
							    &rr_count);
			ops_list[i + j]->status =
			    nandx_chip_get_status(chip, ret, rr_count);
			status = MIN(ops_list[i + j]->status, status);
		}

		if (status == -ENANDREAD)
			break;
	}

	if (chip->randomize)
		nandx_chip_disable_randomizer(chip);

	if (chip->slc_mode)
		slc_mode_exit(chip);

	if (ret == -ENANDREAD) {
		ret = nandx_chip_auto_calibration(chip, cali_count);
		if (ret) {
			cali_count++;
			goto calibration;
		}

		ret = -ENANDREAD;
	}

	if (cali_count > 0)
		nandx_chip_auto_calibration(chip, -1);

	return status;
}

static int nandx_chip_multi_cache_read(struct nandx_chip_dev *chip_dev,
				       struct nandx_ops **ops_list, int count)
{
	int ret, col, status;
	int i, j, num, rr_count, cali_count = 0;
	u32 row;
	void *data, *oob;
	struct nandx_chip *chip;
	struct nandx_ops *ops;
	struct slc_mode_ops *slc_ops;

	return nandx_chip_multi_read(chip_dev, ops_list, count);

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	slc_ops = chip->slc_ops;

calibration:

	if (chip->slc_mode)
		slc_mode_entry(chip, true);

	nandx_chip_multi_read_page(chip, ops_list[0]->row);
	nandx_chip_read_page(chip, ops_list[1]->row);

	for (i = 2; i <= count; i += 2) {
		if (i < count - 1) {
			if (chip->slc_mode)
				slc_mode_entry(chip, false);

			nandx_chip_multi_read_page(chip, ops_list[i]->row);
			nandx_chip_cache_read_page(chip,
						   ops_list[i + 1]->row);
		} else {
			nandx_chip_cache_read_last_page(chip);
		}

		for (j = 0; j < 2; j++) {
			ops = ops_list[i + j - 2];
			num = ops->len / chip->nfc->sector_size;

			row = ops->row;
			col = ops->col;
			if (chip->randomize)
				nandx_chip_enable_randomizer(chip, row,
							     false);

			nandx_chip_random_output(chip, row, col);

			data = ops->data;
			oob = ops->oob;
			ret = nandx_chip_read_data(chip, num, data, oob);
			if (ret == -ENANDREAD) {
				ops = ops_list[i + j];
				ret =
				    nandx_chip_read_retry(chip, ops,
							  &rr_count);
				if (ret == -ENANDREAD)
					break;
				/* break cache read ? */
			}
			ops->status =
			    nandx_chip_get_status(chip, ret, rr_count);
			status = MIN(ops->status, status);
		}

		if (status == -ENANDREAD)
			break;
	}

	if (chip->randomize)
		nandx_chip_disable_randomizer(chip);

	if (chip->slc_mode)
		slc_mode_exit(chip);

	if (ret == -ENANDREAD) {
		ret = nandx_chip_auto_calibration(chip, cali_count);
		if (ret) {
			cali_count++;
			goto calibration;
		}

		ret = -ENANDREAD;
	}

	if (cali_count > 0)
		nandx_chip_auto_calibration(chip, -1);

	return status;
}

static int nandx_chip_program(struct nandx_chip_dev *chip_dev,
			      struct nandx_ops **ops_list, int count)
{
	int ret;
	int i, j, status, cycle;
	u32 row;
	void *data, *oob;
	u8 order_type, wl_page_num;
	struct nandx_chip *chip;
	struct slc_mode_ops *slc_ops;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	slc_ops = chip->slc_ops;
	order_type = chip_dev->program_order_type;
	wl_page_num = chip_dev->info.wl_page_num;

	if (chip->slc_mode) {
		wl_page_num = 1;
		slc_mode_entry(chip, true);
	}

	NANDX_ASSERT((count % wl_page_num) == 0);

	for (i = 0; i < count; i += wl_page_num) {
		for (j = 0; j < wl_page_num; j++) {
			if (!ops_list[i + j])
				continue;

			row = ops_list[i + j]->row;
			data = ops_list[i + j]->data;
			oob = ops_list[i + j]->oob;

			if (!chip->slc_mode &&
			    order_type == PROGRAM_ORDER_TLC) {
				cycle = nandx_chip_get_order_cycle(chip, row);
				nandx_chip_set_program_order_cmd(chip, cycle);
			}

			if (chip->randomize)
				nandx_chip_enable_randomizer(chip, row, true);

			ret = nandx_chip_program_page(chip, row, data, oob);
		}
	}

	if (chip->randomize)
		nandx_chip_disable_randomizer(chip);

	if (chip->slc_mode)
		slc_mode_exit(chip);

	status = nandx_chip_read_status(chip);
	ret = STATUS_FAIL(status) ? -ENANDWRITE : NAND_OK;

	return ret;
}

static int nandx_chip_cache_program(struct nandx_chip_dev *chip_dev,
				    struct nandx_ops **ops_list, int count)
{
	int ret;
	int i, j, status, cycle;
	u32 row;
	void *data, *oob;
	u8 order_type, wl_page_num;
	struct nandx_chip *chip;
	struct nandx_ops *ops;
	struct slc_mode_ops *slc_ops;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	slc_ops = chip->slc_ops;

	order_type = chip_dev->program_order_type;
	wl_page_num = chip_dev->info.wl_page_num;

	if (chip->slc_mode) {
		wl_page_num = 1;
		slc_mode_entry(chip, true);
	}

	NANDX_ASSERT((count % wl_page_num) == 0);

	for (i = 0; i < count; i += wl_page_num) {
		for (j = 0; j < wl_page_num; j++) {
			ops = ops_list[i + j];
			if (!ops)
				continue;

			row = ops->row;
			data = ops->data;
			oob = ops->oob;

			if (!chip->slc_mode &&
			    order_type == PROGRAM_ORDER_TLC) {
				cycle = nandx_chip_get_order_cycle(chip, row);
				nandx_chip_set_program_order_cmd(chip, cycle);
			}

			if (chip->randomize)
				nandx_chip_enable_randomizer(chip, row, true);

			if (chip->slc_mode && i > 0)
				slc_mode_entry(chip, false);

			if (i + j == count - 1)
				ret = nandx_chip_program_page(chip,
							      row, data, oob);
			else
				ret = nandx_chip_cache_program_page(chip,
								    row, data,
								    oob);
		}
	}

	if (chip->randomize)
		nandx_chip_disable_randomizer(chip);

	if (chip->slc_mode)
		slc_mode_exit(chip);

	status = nandx_chip_read_status(chip);
	ret = STATUS_FAIL(status) ? -ENANDWRITE : NAND_OK;

	return ret;
}

static int nandx_chip_multi_program(struct nandx_chip_dev *chip_dev,
				    struct nandx_ops **ops_list, int count)
{
	int ret;
	int i, j, status, step, cycle;
	u32 row;
	void *data, *oob;
	u8 order_type, wl_page_num;
	struct nandx_chip *chip;
	struct nandx_ops *ops;
	struct slc_mode_ops *slc_ops;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	slc_ops = chip->slc_ops;

	order_type = chip_dev->program_order_type;
	wl_page_num = chip_dev->info.wl_page_num;

	if (chip->slc_mode) {
		wl_page_num = 1;
		slc_mode_entry(chip, true);
	}

	step = wl_page_num << 1;
	NANDX_ASSERT((count % step) == 0);

	for (i = 0; i < count; i += step) {
		for (j = 0; j < wl_page_num; j++) {
			ops = ops_list[i + j * 2];
			if (!ops)
				continue;

			row = ops->row;
			data = ops->data;
			oob = ops->oob;

			if (!chip->slc_mode &&
			    order_type == PROGRAM_ORDER_TLC) {
				cycle = nandx_chip_get_order_cycle(chip, row);
				nandx_chip_set_program_order_cmd(chip, cycle);
			}

			if (chip->randomize)
				nandx_chip_enable_randomizer(chip, row, true);
			if (chip->slc_mode && i > 0)
				slc_mode_entry(chip, false);

			ret =
			    nandx_chip_multi_program_1stpage(chip, row, data,
							     oob);
			if (ret < 0)
				goto out;

			ops = ops_list[i + j * 2 + 1];
			row = ops->row;
			data = ops->data;
			oob = ops->oob;

			if (!chip->slc_mode &&
			    order_type == PROGRAM_ORDER_TLC) {
				cycle = nandx_chip_get_order_cycle(chip, row);
				nandx_chip_set_program_order_cmd(chip, cycle);
			}

			if (chip->randomize)
				nandx_chip_enable_randomizer(chip, row, true);

			if (j == wl_page_num - 1 || chip->slc_mode)
				ret = nandx_chip_program_page(chip, row,
							      data, oob);
			else
				ret = nandx_chip_multi_program_2ndpage(chip,
								       row,
								       data,
								       oob);
			if (ret < 0)
				goto out;
		}
	}

	if (chip->randomize)
		nandx_chip_disable_randomizer(chip);

	if (chip->slc_mode)
		slc_mode_exit(chip);

	status = nandx_chip_read_status(chip);
	ret = STATUS_FAIL(status) ? -ENANDWRITE : NAND_OK;

out:
	return ret;
}

/* Todo: refine for 2D MLC later. */
static int nandx_chip_multi_cache_program(struct nandx_chip_dev *chip_dev,
					  struct nandx_ops **ops_list,
					  int count)
{
	int ret;
	int i, j, status, step, cycle;
	u8 order_type, wl_page_num;
	u32 row;
	void *data, *oob;
	struct nandx_chip *chip;
	struct nandx_ops *ops;
	struct slc_mode_ops *slc_ops;

	return nandx_chip_multi_program(chip_dev, ops_list, count);

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	slc_ops = chip->slc_ops;

	order_type = chip_dev->program_order_type;
	wl_page_num = chip_dev->info.wl_page_num;

	if (chip->slc_mode) {
		wl_page_num = 1;
		slc_mode_entry(chip, true);
	}

	step = wl_page_num << 1;
	NANDX_ASSERT((count % step) == 0);

	for (i = 0; i < count; i += step) {
		for (j = 0; j < wl_page_num; j++) {
			ops = ops_list[i + j * 2];
			if (!ops)
				continue;

			row = ops->row;
			data = ops->data;
			oob = ops->oob;

			if (!chip->slc_mode &&
			    order_type == PROGRAM_ORDER_TLC) {
				cycle = nandx_chip_get_order_cycle(chip, row);
				nandx_chip_set_program_order_cmd(chip, cycle);
			}

			if (chip->randomize)
				nandx_chip_enable_randomizer(chip, row, true);
			if (chip->slc_mode && i > 0)
				slc_mode_entry(chip, false);

			ret =
			    nandx_chip_multi_program_1stpage(chip, row, data,
							     oob);

			ops = ops_list[i + j * 2 + 1];
			row = ops->row;
			data = ops->data;
			oob = ops->oob;

			if (!chip->slc_mode &&
			    order_type == PROGRAM_ORDER_TLC) {
				cycle = nandx_chip_get_order_cycle(chip, row);
				nandx_chip_set_program_order_cmd(chip, cycle);
			}

			if (chip->randomize)
				nandx_chip_enable_randomizer(chip, row, true);

			/* last program and last page */
			if (j == wl_page_num - 1 && i == count - step)
				ret = nandx_chip_program_page(chip, row, data,
							      oob);
			/* last page of plane1 wl */
			else if (j == wl_page_num - 1)
				ret = nandx_chip_cache_program_page(chip, row,
								    data,
								    oob);
			else
				ret = nandx_chip_multi_program_2ndpage(chip,
								       row,
								       data,
								       oob);
		}
	}

	if (chip->randomize)
		nandx_chip_disable_randomizer(chip);

	if (chip->slc_mode)
		slc_mode_exit(chip);

	status = nandx_chip_read_status(chip);
	ret = STATUS_FAIL(status) ? -ENANDWRITE : NAND_OK;

	return ret;
}

static int nandx_chip_erase(struct nandx_chip_dev *chip_dev, u32 row)
{
	u8 status;
	struct nandx_chip *chip;
	struct slc_mode_ops *slc_ops;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	slc_ops = chip->slc_ops;

	if (chip->slc_mode)
		slc_mode_entry(chip, true);

	nandx_chip_erase_block(chip, row);
	status = nandx_chip_read_status(chip);

	if (chip->slc_mode)
		slc_mode_exit(chip);

	return STATUS_FAIL(status) ? -ENANDERASE : NAND_OK;
}

static int nandx_chip_multi_erase(struct nandx_chip_dev *chip_dev, u32 *rows)
{
	u8 status;
	struct nandx_chip *chip;
	struct slc_mode_ops *slc_ops;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	slc_ops = chip->slc_ops;

	if (chip->slc_mode)
		slc_mode_entry(chip, true);

	nandx_chip_multi_erase_block(chip, rows);
	status = nandx_chip_read_status(chip);

	if (chip->slc_mode)
		slc_mode_exit(chip);

	return STATUS_FAIL(status) ? -ENANDERASE : NAND_OK;
}

static int nandx_chip_multi_plane_check(struct nandx_chip_dev *chip_dev,
					u32 *rows)
{
	u32 block, plane;
	u32 page_per_block, i;
	struct nandx_chip *chip;
	struct nandx_device_info *dev_info;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	dev_info = chip->dev_info;

	page_per_block = dev_info->block_size / dev_info->page_size;

	for (i = 0; i < dev_info->plane_num; i++) {
		block = rows[i] / page_per_block;
		plane = block % dev_info->plane_num;
		if (plane != i)
			return -1;
	}

	return 0;
}

static bool nandx_chip_block_is_bad(struct nandx_chip_dev *chip_dev, u32 row)
{
	int ret;
	struct nandx_chip *chip;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);

	ret =
	    nandx_bad_block_check(chip, row, chip->dev_info->bad_block_type);

	return ret == NAND_OK ? false : true;
}

static int nandx_interface_change(struct nandx_chip *chip, bool enable,
				  void *arg)
{
	struct nfc_handler *nfc = chip->nfc;
	struct nandx_device_info *dev_info = chip->dev_info;
	enum INTERFACE_TYPE type = INTERFACE_LEGACY;
	u8 addr = chip->feature[FEATURE_INTERFACE_CHANGE];
	u8 feature[4] = { 0 };
	u8 timing_mode = 4; /* fixed legaced timing mode need Todo */
	int ret;

	if (!chip->interface_ddr && enable)
		return -EOPNOTSUPP;

	if (addr == NONE)
		return 0;

	if (enable) {
		type = dev_info->interface_type;
		timing_mode = dev_info->ddr_clock_type;
	}

	feature[0] = get_nandx_interface_value(chip->vendor_type,
					       timing_mode, type);

	nandx_chip_set_feature(chip, addr, feature, 4);

	ret = nfc->change_interface(nfc, type, &chip->timing, arg);

	chip->ddr_enable = enable;

	return ret;
}

static int nandx_chip_change_mode(struct nandx_chip_dev *chip_dev,
				  enum OPS_MODE_TYPE mode,
				  bool enable, void *arg)
{
	int ret = NAND_OK;
	struct nandx_chip *chip;
	struct nfc_handler *nfc;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	nfc = chip->nfc;

	switch (mode) {
	case OPS_MODE_SLC:
		chip->slc_mode = enable;
		break;

	case OPS_MODE_DDR:
		ret = nandx_interface_change(chip, enable, arg);
		break;

	case OPS_MODE_RANDOMIZE:
		chip->randomize = enable;
		break;

	case OPS_MODE_CALIBRATION:
		chip->calibration = enable;
		break;

	default:
		ret = nfc->change_mode(nfc, mode, enable, arg);
		break;
	}

	return ret;
}

static bool nandx_chip_get_mode(struct nandx_chip_dev *chip_dev,
				enum OPS_MODE_TYPE mode)
{
	bool ret = false;
	struct nandx_chip *chip;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);

	switch (mode) {
	case OPS_MODE_SLC:
		ret = chip->slc_mode;
		break;

	case OPS_MODE_DDR:
		ret = chip->ddr_enable;
		break;

	case OPS_MODE_RANDOMIZE:
		ret = chip->randomize;
		break;

	case OPS_MODE_CALIBRATION:
		ret = chip->calibration;
		break;

	default:
		ret = chip->nfc->get_mode(chip->nfc, mode);
		break;
	}

	return ret;
}

static int nandx_chip_suspend(struct nandx_chip_dev *chip_dev)
{
	struct nandx_chip *chip;
	struct nfc_handler *nfc;
	int ret = 0;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	nfc = chip->nfc;

	if (nfc->suspend)
		ret = nfc->suspend(nfc);

	return ret;
}

static int nandx_chip_resume(struct nandx_chip_dev *chip_dev)
{
	struct nandx_chip *chip;
	struct nfc_handler *nfc;
	int ret = 0;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	nfc = chip->nfc;

	if (nfc->resume)
		ret = nfc->resume(nfc);

	udelay(200);
	nandx_chip_reset(chip);

	/* nfc goes to async mode after power on */
	chip->ddr_enable = false;

	return ret;
}

static void nandx_chip_dev_alloc(struct nandx_chip_dev *chip_dev)
{
	struct nandx_chip *chip;
	struct nfc_handler *nfc;
	struct nandx_device_info *dev_info;
	struct nandx_chip_info *info;
	u8 order_type;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	nfc = chip->nfc;
	dev_info = chip->dev_info;
	info = &chip_dev->info;

	info->plane_num = dev_info->plane_num;
	info->block_num = dev_info->target_num * dev_info->lun_num;
	info->block_num *= dev_info->plane_num * dev_info->block_num;
	info->block_size = dev_info->block_size;
	info->page_size = dev_info->page_size;
	info->oob_size =
	    dev_info->page_size / nfc->sector_size * nfc->fdm_size;
	info->sector_size = chip->nfc->sector_size;
	info->ecc_strength = chip->nfc->ecc_strength;

	order_type = PROGRAM_ORDER_NONE;

	switch (dev_info->type) {
	case NAND_SLC:
		chip_dev->info.wl_page_num = 1;
		chip_dev->info.slc_block_size = dev_info->block_size;
		break;

	case NAND_MLC:
	case NAND_VMLC:
		chip_dev->info.wl_page_num = 2;
		chip_dev->info.slc_block_size = dev_info->block_size >> 1;
		break;

	case NAND_TLC:
		order_type = PROGRAM_ORDER_TLC;
	case NAND_VTLC:
		chip_dev->info.wl_page_num = 3;
		chip_dev->info.slc_block_size = dev_info->block_size / 3;
		if (order_type == PROGRAM_ORDER_NONE) {
			if (chip->vendor_type == VENDOR_TOSHIBA)
				order_type = PROGRAM_ORDER_VTLC_TOSHIBA;
			else if (chip->vendor_type == VENDOR_MICRON)
				order_type = PROGRAM_ORDER_VTLC_MICRON;
		}
		break;

	default:
		break;
	}

	chip_dev->program_order_type = order_type;
	chip_dev->read_page = nandx_chip_read;
	chip_dev->cache_read_page = nandx_chip_cache_read;
	chip_dev->multi_read_page = nandx_chip_multi_read;
	chip_dev->multi_cache_read_page = nandx_chip_multi_cache_read;
	chip_dev->program_page = nandx_chip_program;
	chip_dev->cache_program_page = nandx_chip_cache_program;
	chip_dev->multi_program_page = nandx_chip_multi_program;
	chip_dev->multi_cache_program_page = nandx_chip_multi_cache_program;
	chip_dev->erase = nandx_chip_erase;
	chip_dev->multi_erase = nandx_chip_multi_erase;
	chip_dev->multi_plane_check = nandx_chip_multi_plane_check;
	chip_dev->block_is_bad = nandx_chip_block_is_bad;
	chip_dev->change_mode = nandx_chip_change_mode;
	chip_dev->get_mode = nandx_chip_get_mode;
	chip_dev->suspend = nandx_chip_suspend;
	chip_dev->resume = nandx_chip_resume;
}

static int nandx_chip_program_order_alloc(struct nandx_chip *chip)
{
	struct nandx_device_info *dev_info = chip->dev_info;

	if (chip->chip_dev.program_order_type == PROGRAM_ORDER_TLC) {
		chip->orders =
		    mem_alloc(chip->chip_dev.info.wl_page_num *
			      dev_info->plane_num,
			      sizeof(struct program_order));
		if (!chip->orders) {
			NANDX_ASSERT(0);
			return -ENOMEM;
		}

		memset(chip->orders, 0,
		       chip->chip_dev.info.wl_page_num * dev_info->plane_num *
		       sizeof(struct program_order));
	}

	return 0;
}

static int nandx_chip_vendor_info_alloc(struct nandx_chip *chip, u8 id)
{
	u8 *str = NULL;

	chip->vendor_type = get_vendor_type(id);
	chip->pre_cmd = EXTEND_CMD_NONE;
	chip->program_order_cmd = EXTEND_CMD_NONE;
	chip->feature = get_nandx_feature_table(chip->vendor_type);

	switch (chip->vendor_type) {
	case VENDOR_SAMSUNG:
		break;

	case VENDOR_TOSHIBA:
		str = get_nandx_drive_strength_table(DRIVE_STRENGTH_TOSHIBA);
		if (chip->dev_info->type == NAND_TLC) {
			chip->pre_cmd = EXTEND_TLC_PRE_CMD;
			chip->program_order_cmd =
			    EXTEND_TLC_PROGRAM_ORDER_CMD;
		}
		break;

	case VENDOR_SANDISK:
		str = get_nandx_drive_strength_table(DRIVE_STRENGTH_TOSHIBA);
		if (chip->dev_info->type == NAND_TLC) {
			chip->pre_cmd = EXTEND_TLC_PRE_CMD;
			chip->program_order_cmd =
			    EXTEND_TLC_PROGRAM_ORDER_CMD;
		}
		break;

	case VENDOR_HYNIX:
		str = get_nandx_drive_strength_table(DRIVE_STRENGTH_HYNIX);
		break;

	case VENDOR_MICRON:
		str = get_nandx_drive_strength_table(DRIVE_STRENGTH_MICRON);
		break;

	default:
		break;
	}

	chip->drive_strength = str;
	set_replace_cmd(chip->vendor_type);

	return nandx_chip_program_order_alloc(chip);
}

static void nandx_chip_get_interface_timing(struct nandx_chip *chip)
{
	u8 sdr_type, ddr_type;
	struct nandx_device_info *dev_info = chip->dev_info;

	sdr_type = dev_info->sdr_timing_type;
	ddr_type = dev_info->ddr_timing_type;

	chip->timing.legacy = get_nandx_timing(INTERFACE_LEGACY, sdr_type);
	chip->interface_ddr = false;
	chip->timing.ddr_clock = 0;

	switch (dev_info->interface_type) {
	case INTERFACE_ONFI:
		chip->timing.ddr.onfi = get_nandx_timing(INTERFACE_ONFI,
							 ddr_type);
		break;

	case INTERFACE_TOGGLE:
		chip->timing.ddr.toggle =
		    get_nandx_timing(INTERFACE_TOGGLE, ddr_type);
		break;

	default:
		chip->timing.ddr.onfi = NULL;
		chip->timing.ddr.toggle = NULL;
		break;
	}

	if (chip->timing.ddr.onfi || chip->timing.ddr.toggle) {
		chip->timing.ddr_clock =
		    get_nandx_ddr_clock(dev_info->interface_type,
					dev_info->ddr_clock_type);
		chip->interface_ddr = true;
	}
}

struct nandx_chip_dev *nandx_chip_alloc(struct nfc_resource *res)
{
	u8 id[ID_MAX_NUM] = { 0 };
	u8 addr_table_type;
	struct nandx_chip *chip;
	struct nandx_device_info *dev_info;
	struct nfc_format format;

	chip = mem_alloc(1, sizeof(struct nandx_chip));
	if (!chip) {
		NANDX_ASSERT(0);
		return NULL;
	}

	chip->cmd = get_basic_cmd_sets();
	chip->nfc = nfc_init(res);

	nandx_chip_reset(chip);
	nandx_chip_read_id(chip, id, ID_MAX_NUM);

	chip->dev_info = get_nandx_device_info(id, ID_MAX_NUM);
	if (!chip->dev_info) {
		/* TODO:  read parameter page */
		pr_warn("Get nand device information fail\n");
		return NULL;
	}

	format.page_size = chip->dev_info->page_size;
	format.oob_size = chip->dev_info->spare_size;
	format.ecc_strength = chip->dev_info->xlc_life.ecc_required;
	chip->nfc->set_format(chip->nfc, &format);

	nandx_chip_dev_alloc(&chip->chip_dev);
	if (nandx_chip_vendor_info_alloc(chip, id[0])) {
		pr_warn("Chip vendor info alloc fail\n");
		return NULL;
	}

	dev_info = chip->dev_info;

	addr_table_type = dev_info->address_table_type;

	chip->addressing = get_nandx_addressing_table(addr_table_type);
	chip->pp_ops = get_pair_page_ops(dev_info->mode_type);
	chip->slc_ops = get_slc_mode_ops(dev_info->mode_type);
	chip->rr_ops = get_read_retry_ops(dev_info);

	nandx_chip_get_interface_timing(chip);

	chip->slc_mode = false;
	chip->ddr_enable = false;
	chip->calibration = false;
	chip->randomize = true;
	chip->rr_count = 0;

#ifdef CONFIG_NANDX_EXPRESS
	nandx_express_chip = chip;
#endif

	pr_info("version: %s\n", NANDX_CHIP_VERSION);

	/* Todo for new chip dev */
	return &chip->chip_dev;
}

void nandx_chip_free(struct nandx_chip_dev *chip_dev)
{
	struct nandx_chip *chip;

	chip = container_of(chip_dev, struct nandx_chip, chip_dev);
	if (chip->orders)
		mem_free(chip->orders);

	mem_free(chip);
}
