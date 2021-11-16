/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */

#include "nandx_errno.h"
#include "nandx_chip_common.h"
#include "nandx_device_info.h"

#define RR_MAX_PARAM_NUM	(8)
#define RR_FEATURE_NUM		(4)

/**
 * Modify with spec define and sync with nandx_device_info table
 */
static void rr_none(struct nandx_chip *chip)
{
}

static inline u16 get_loop_count(struct nandx_chip *chip)
{
	struct read_retry_ops *rr_ops = chip->rr_ops;
	bool slc_mode = chip->slc_mode;

	return slc_mode ? rr_ops->slc_loop_count : rr_ops->loop_count;
}

/* MICRON READ RETRY Start */
static void micron_rr_get_parameters(struct nandx_chip *chip, u8 *param)
{
	u16 rr_loop_count;

	param[0] = chip->rr_count;
	chip->rr_count++;

	rr_loop_count = get_loop_count(chip);
	if (chip->rr_count >= rr_loop_count)
		chip->rr_count = 0;
}

static void l95b_rr_get_parameters(struct nandx_chip *chip, u8 *param)
{
	u16 rr_loop_count;

	param[0] = chip->rr_count;
	chip->rr_count++;

	rr_loop_count = get_loop_count(chip) - 1;
	if (chip->rr_count == rr_loop_count)
		chip->rr_count = 12;
	else if (chip->rr_count > rr_loop_count)
		chip->rr_count = 0;
}

static void micron_rr_set_parameters(struct nandx_chip *chip, u8 *param)
{
	int ret;
	u8 param_back[RR_FEATURE_NUM];
	u8 feature = chip->feature[FEATURE_READ_RETRY];

	ret = nandx_chip_set_feature_with_check(chip, feature, param,
						param_back, RR_FEATURE_NUM);
	NANDX_ASSERT(!ret);
}

static void micron_rr_exit(struct nandx_chip *chip)
{
	int ret;
	u8 param[RR_FEATURE_NUM] = { 0 };
	u8 param_back[RR_FEATURE_NUM] = { 0xff };
	u8 feature = chip->feature[FEATURE_READ_RETRY];

	chip->rr_count = 0;

	pr_debug("micron rr exit\n");
	ret = nandx_chip_set_feature_with_check(chip, feature, param,
						param_back, RR_FEATURE_NUM);
	NANDX_ASSERT(!ret);
}

/* MICRON READ RETRY End */

/* MLC SANDISK READ RETRY Start */
#define MLC_SANDISK_SETS_NUM	(148)
static void mlc_sandisk_rr_get_parameters(struct nandx_chip *chip, u8 *param)
{
	u16 i, index;
	u8 rr_setting[MLC_SANDISK_SETS_NUM] = {
		/* P0, P1, P2, P3 */
		0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x7c, 0x04, 0x00,
		0x7c, 0x78,
		0x78, 0x00, 0x78, 0x74, 0x08, 0x7c, 0x00, 0x7c, 0x00, 0x7c,
		0x7c, 0x78,
		0x7c, 0x7c, 0x78, 0x74, 0x00, 0x7c, 0x74, 0x70, 0x00, 0x78,
		0x00, 0x7c,
		0x00, 0x78, 0x7c, 0x78, 0x00, 0x78, 0x78, 0x74, 0x00, 0x78,
		0x74, 0x70,
		0x00, 0x78, 0x70, 0x6c, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04,
		0x00, 0x7c,
		0x0c, 0x04, 0x7c, 0x78, 0x0c, 0x04, 0x78, 0x74, 0x10, 0x08,
		0x00, 0x7c,
		0x10, 0x08, 0x04, 0x00, 0x0c, 0x0c, 0x04, 0x04, 0x10, 0x0c,
		0x04, 0x00,
		0x14, 0x10, 0x08, 0x00, 0x18, 0x14, 0x0c, 0x00, 0x0c, 0x0c,
		0x04, 0x7c,
		0x78, 0x74, 0x78, 0x74, 0x78, 0x74, 0x74, 0x70, 0x78, 0x74,
		0x70, 0x6c,
		0x78, 0x74, 0x6c, 0x68, 0x78, 0x70, 0x78, 0x74, 0x78, 0x70,
		0x74, 0x70,
		0x78, 0x70, 0x6c, 0x68, 0x78, 0x70, 0x70, 0x6c, 0x78, 0x6c,
		0x70, 0x6c,
		0x78, 0x6c, 0x6c, 0x68, 0x78, 0x6c, 0x68, 0x64, 0x74, 0x68,
		0x6c, 0x68,
		0x74, 0x68, 0x68, 0x64
	};

	index = chip->rr_count % MLC_SANDISK_SETS_NUM;
	for (i = 0; i < RR_FEATURE_NUM; i++)
		param[i] = rr_setting[index + i];

	chip->rr_count += RR_FEATURE_NUM;
}

static void mlc_sandisk_rr_set_parameters(struct nandx_chip *chip, u8 *param)
{
	int ret;
	u8 param_back[RR_FEATURE_NUM] = { 0xff };
	u8 feature = chip->feature[FEATURE_READ_RETRY];

	ret = nandx_chip_set_feature_with_check(chip, feature, param,
						param_back, RR_FEATURE_NUM);
	NANDX_ASSERT(!ret);
}

static void sandisk_rr_enable(struct nandx_chip *chip)
{
	chip->nfc->send_command(chip->nfc, 0x5d);
}

static void mlc_sandisk_rr_enable(struct nandx_chip *chip)
{
	struct nfc_handler *nfc = chip->nfc;
	u16 rr_case = chip->rr_count / MLC_SANDISK_SETS_NUM;

	if (rr_case == 3) {
		nfc->send_command(nfc, 0x5c);
		nfc->send_command(nfc, 0xc5);
		nfc->send_command(nfc, 0x55);
		nfc->send_address(nfc, 0, 0, 1, 0);
		nfc->write_byte(nfc, 0x01);
		nfc->send_command(nfc, 0x55);
		nfc->send_address(nfc, 0x23, 0, 1, 0);
		nfc->write_byte(nfc, 0xc0);
	}

	if (rr_case > 1)
		nfc->send_command(nfc, 0x25);

	sandisk_rr_enable(chip);

	if (rr_case == 1 || rr_case == 3)
		nfc->send_command(nfc, 0x26);
}

static void mlc_sandisk_rr_exit(struct nandx_chip *chip)
{
	u8 param[RR_MAX_PARAM_NUM] = { 0 };

	chip->rr_count = 0;
	mlc_sandisk_rr_set_parameters(chip, param);
	nandx_chip_reset(chip);
}

/* MLC SANDISK READ RETRY End */

/* MLC SANDISK 19NM READ RETRY Start */
#define MLC_SANDISK_19NM_SETS_NUM	(42)
static void mlc_sandisk_19nm_rr_entry(struct nandx_chip *chip)
{
	int i;
	struct nfc_handler *nfc = chip->nfc;

	/* change nfc timming ? */
	nfc->send_command(nfc, 0x3b);
	nfc->send_command(nfc, 0xb9);
	for (i = 0; i < 9; i++) {
		nfc->send_command(nfc, 0x53);
		nfc->send_address(nfc, 0x04 + i, 0, 1, 0);
		nfc->write_byte(nfc, 0x00);
	}
	/* restore nfc timming ? */
}

static void mlc_sandisk_19nm_rr_get_parameters(struct nandx_chip *chip,
					       u8 *param)
{
	u16 rr_loop_count;
	u8 rr_setting[MLC_SANDISK_19NM_SETS_NUM] = {
		/* 04, 05, 07 */
		0x00, 0x00, 0xf0, 0xff, 0xef, 0xee,
		0xdf, 0xdd, 0x1e, 0xe1, 0x2e, 0xd2,
		0x3d, 0xf3, 0xcd, 0xed, 0x0d, 0xd1,
		0x01, 0x12, 0x12, 0x22, 0xb2, 0x1d,
		0xa3, 0x2d, 0x9f, 0x0d, 0xbe, 0xfc,
		0xad, 0xcc, 0x9f, 0xfc, 0x01, 0x00,
		0x02, 0x00, 0x0d, 0xb0, 0x0c, 0xa0
	};

	param[0] = rr_setting[chip->rr_count];
	param[1] = rr_setting[chip->rr_count + 1] & 0xf0;
	param[2] = rr_setting[chip->rr_count + 1] << 4 & 0xf0;

	chip->rr_count += 2;

	rr_loop_count = get_loop_count(chip) << 1;
	if (chip->rr_count >= rr_loop_count)
		chip->rr_count = 0;
}

static void mlc_sandisk_19nm_rr_set_parameters(struct nandx_chip *chip,
					       u8 *param)
{
	int i;
	u8 address[3] = { 0x04, 0x05, 0x07 };
	struct nfc_handler *nfc = chip->nfc;

	/* change nfc timming ? */
	nfc->send_command(nfc, 0x3b);
	nfc->send_command(nfc, 0xb9);

	for (i = 0; i < 3; i++) {
		nfc->send_command(nfc, 0x53);
		nfc->send_address(nfc, address[i], 0, 1, 0);
		nfc->write_byte(nfc, param[i]);
	}

	/* restore nfc timming ? */
}

static void mlc_sandisk_19nm_rr_enable(struct nandx_chip *chip)
{
	chip->nfc->send_command(chip->nfc, 0xb6);
}

static void mlc_sandisk_19nm_rr_exit(struct nandx_chip *chip)
{
	u8 param[RR_MAX_PARAM_NUM] = { 0 };
	struct nfc_handler *nfc = chip->nfc;

	chip->rr_count = 0;
	nfc->send_command(nfc, 0xd6);
	mlc_sandisk_19nm_rr_set_parameters(chip, param);
	nandx_chip_reset(chip);
}

/* MLC SANDISK 19NM READ RETRY End */

/* MLC SANDISK 1ZNM READ RETRY Start */
#define MLC_SANDISK_1ZNM_8GB_SETS_NUM		(96)
#define MLC_SANDISK_1ZNM_16GB_SETS_NUM		(132)
#define SLC_SANDISK_1ZNM_SETS_NUM			(25)
static void mlc_sandisk_1znm_rr_get_parameters(struct nandx_chip *chip,
					       u8 *param)
{
	struct nandx_device_info *info = chip->dev_info;
	u64 total_size;
	u16 rr_loop_count, i;
	u8 *mlc_rr_setting;
	u8 mlc_8gb_rr_setting[MLC_SANDISK_1ZNM_8GB_SETS_NUM] = {
		0x00, 0x00, 0x00, 0x04, 0x78, 0x78,
		0x04, 0x7c, 0x74, 0x00, 0x04, 0x78,
		0x04, 0x7c, 0x7c, 0x00, 0x00, 0x7c,
		0x00, 0x00, 0x74, 0x08, 0x04, 0x78,
		0x7c, 0x04, 0x78, 0x7c, 0x00, 0x7c,
		0x04, 0x7c, 0x70, 0x7c, 0x74, 0x74,
		0x00, 0x78, 0x70, 0x0c, 0x08, 0x78,
		0x78, 0x7c, 0x7c, 0x04, 0x08, 0x04,
		0x78, 0x08, 0x78, 0x7c, 0x78, 0x70,
		0x78, 0x70, 0x6c, 0x00, 0x74, 0x6c,
		0x08, 0x00, 0x74, 0x7c, 0x78, 0x6c,
		0x00, 0x04, 0x04, 0x74, 0x74, 0x6c,
		0x78, 0x7c, 0x70, 0x0c, 0x00, 0x74,
		0x04, 0x0c, 0x08, 0x78, 0x7c, 0x74,
		0x78, 0x70, 0x68, 0x08, 0x00, 0x70,
		0x10, 0x0c, 0x78, 0x00, 0x0c, 0x08
	};
	u8 mlc_16gb_rr_setting[MLC_SANDISK_1ZNM_16GB_SETS_NUM] = {
		0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00,
		0x7c, 0x7c, 0x00, 0x00, 0x04, 0x04, 0x7c, 0x7c,
		0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x7c,
		0x08, 0x08, 0x7c, 0x7c, 0x04, 0x04, 0x08, 0x04,
		0x00, 0x00, 0x04, 0x04, 0x7c, 0x7c, 0x00, 0x7c,
		0x08, 0x08, 0x08, 0x04, 0x0c, 0x04, 0x7c, 0x78,
		0x00, 0x00, 0x78, 0x78, 0x0c, 0x0c, 0x00, 0x00,
		0x78, 0x78, 0x00, 0x00, 0x7c, 0x7c, 0x00, 0x04,
		0x78, 0x78, 0x74, 0x7c, 0x7c, 0x7c, 0x78, 0x78,
		0x04, 0x04, 0x04, 0x08, 0x08, 0x0c, 0x08, 0x04,
		0x08, 0x08, 0x08, 0x08, 0x78, 0x78, 0x78, 0x78,
		0x7c, 0x7c, 0x7c, 0x00, 0x08, 0x04, 0x04, 0x00,
		0x00, 0x00, 0x08, 0x00, 0x04, 0x08, 0x78, 0x00,
		0x08, 0x0c, 0x78, 0x7c, 0x7c, 0x08, 0x74, 0x78,
		0x7c, 0x7c, 0x78, 0x74, 0x00, 0x00, 0x74, 0x74,
		0x00, 0x00, 0x00, 0x08, 0x78, 0x78, 0x7c, 0x74,
		0x78, 0x04, 0x70, 0x74
	};
	u8 slc_rr_setting[SLC_SANDISK_1ZNM_SETS_NUM] = {
		0x00, 0x04, 0x7c, 0x08, 0x78, 0x0c, 0x74, 0x10,
		0x70, 0x14, 0x6c, 0x18, 0x68, 0x1c, 0x64, 0x20,
		0x60, 0x24, 0x5c, 0x28, 0x58, 0x2c, 0x30, 0x34, 0x38
	};

	total_size = (u64)info->block_num * info->block_size;
	mlc_rr_setting = mlc_8gb_rr_setting;
	if ((u32)(total_size >> 10) > MB(8))
		mlc_rr_setting = mlc_16gb_rr_setting;

	if (chip->slc_mode) {
		param[0] = slc_rr_setting[chip->rr_count];
		chip->rr_count += 1;
		rr_loop_count = chip->rr_ops->slc_loop_count;
	} else if ((u32)(total_size >> 10) > MB(8)) {
		for (i = 0; i < RR_FEATURE_NUM; i++)
			param[i] = mlc_rr_setting[chip->rr_count + i];
		chip->rr_count += RR_FEATURE_NUM;
		rr_loop_count = chip->rr_ops->loop_count << 2;
	} else {
		param[0] = mlc_rr_setting[chip->rr_count];
		param[1] = param[0];
		param[2] = mlc_rr_setting[chip->rr_count + 1];
		param[3] = mlc_rr_setting[chip->rr_count + 2];
		chip->rr_count += 3;
		rr_loop_count = chip->rr_ops->loop_count * 3;
	}

	if (chip->rr_count >= rr_loop_count)
		chip->rr_count = 0;
}

/* MLC SANDISK 1ZNM READ RETRY End */

/* MLC TOSHIBA 19NM READ RETRY Start */
#define MLC_TOSHIBA_19NM_SETS_NUM	(35)
static void mlc_toshiba_19nm_rr_entry(struct nandx_chip *chip)
{
	struct nfc_handler *nfc = chip->nfc;

	nfc->send_command(nfc, 0x5c);
	nfc->send_command(nfc, 0xc5);
}

static void mlc_toshiba_19nm_rr_get_parameters(struct nandx_chip *chip,
					       u8 *param)
{
	u16 i, rr_loop_count;
	u8 rr_setting[MLC_TOSHIBA_19NM_SETS_NUM] = {
		0x04, 0x04, 0x7C, 0x7E, 0x00,
		0x00, 0x7C, 0x78, 0x78, 0x00,
		0x7C, 0x76, 0x74, 0x72, 0x00,
		0x08, 0x08, 0x00, 0x00, 0x00,
		0x0B, 0x7E, 0x76, 0x74, 0x00,
		0x10, 0x76, 0x72, 0x70, 0x00,
		0x02, 0x7C, 0x7E, 0x70, 0x00
	};

	for (i = 0; i < 5; i++)
		param[i] = rr_setting[chip->rr_count + i];

	chip->rr_count += 5;
	rr_loop_count = get_loop_count(chip) * 5;
	if (chip->rr_count >= rr_loop_count)
		chip->rr_count = 0;
}

static void mlc_toshiba_19nm_rr_set_parameters(struct nandx_chip *chip,
					       u8 *param)
{
	int i;
	struct nfc_handler *nfc = chip->nfc;
	u8 address[5] = { 0x04, 0x05, 0x06, 0x07, 0x0d };

	for (i = 0; i < 5; i++) {
		nfc->send_command(nfc, 0x55);
		nfc->send_address(nfc, address[i], 0, 1, 0);
		nfc->write_byte(nfc, param[i]);
	}
}

static void mlc_toshiba_19nm_rr_enable(struct nandx_chip *chip)
{
	struct nfc_handler *nfc = chip->nfc;

	if (chip->rr_count == 4 * 5)
		nfc->send_command(nfc, 0xb3);
	nfc->send_command(nfc, 0x26);
	nfc->send_command(nfc, 0x5d);
}

static void mlc_toshiba_19nm_rr_exit(struct nandx_chip *chip)
{
	u8 param[RR_MAX_PARAM_NUM] = { 0 };

	chip->rr_count = 0;
	mlc_toshiba_19nm_rr_set_parameters(chip, param);
	nandx_chip_reset(chip);
}

/* MLC TOSHIBA 19NM READ RETRY End */

/* MLC TOSHIBA 15NM READ RETRY Start */
#define MLC_TOSHIBA_15NM_SETS_NUM	(50)
static void mlc_toshiba_15nm_rr_get_parameters(struct nandx_chip *chip,
					       u8 *param)
{
	u16 i, rr_loop_count;
	u8 rr_setting[MLC_TOSHIBA_15NM_SETS_NUM] = {
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x04, 0x02, 0x00, 0x00,
		0x7C, 0x00, 0x7C, 0x7C, 0x00,
		0x7A, 0x00, 0x7A, 0x7A, 0x00,
		0x78, 0x02, 0x78, 0x7A, 0x00,
		0x7E, 0x04, 0x7E, 0x7A, 0x00,
		0x76, 0x04, 0x76, 0x78, 0x00,
		0x04, 0x04, 0x04, 0x76, 0x00,
		0x06, 0x0A, 0x06, 0x02, 0x00,
		0x74, 0x7C, 0x74, 0x76, 0x00
	};

	for (i = 0; i < 5; i++)
		param[i] = rr_setting[chip->rr_count + i];

	chip->rr_count += 5;
	rr_loop_count = get_loop_count(chip) * 5;
	if (chip->rr_count >= rr_loop_count)
		chip->rr_count = 0;
}

static void mlc_toshiba_15nm_rr_enable(struct nandx_chip *chip)
{
	struct nfc_handler *nfc = chip->nfc;

	if (chip->rr_count == 5)
		nfc->send_command(nfc, 0x26);
	else
		nfc->send_command(nfc, 0xcd);
	nfc->send_command(nfc, 0x5d);
}

/* MLC TOSHIBA 15NM READ RETRY End */

/* MLC HYNIX 20/1X/16NM READ RETRY Start */
static u8 *mlc_hynix_rr_table_read(struct nandx_chip *chip)
{
	static u8 *table_data;
	static u32 table_offset;
	static bool only_once;
	struct nfc_handler *nfc = chip->nfc;
	u8 rr_type = chip->dev_info->read_retry_type;
	u32 table_size = 0, set_size = 0, set_offset = 0;
	u8 addr_1st[2] = { 0xff, 0xcc };
	u8 data[2] = { 0x40, 0x4d };
	u8 command[5] = { 0x16, 0x17, 0x04, 0x19, 0x00 };
	u8 addr_2nd[5] = { 0x00, 0x00, 0x00, 0x02, 0x00 };
	u32 index = 0, count;
	u8 *origin, *inverse;

	if (only_once)
		return table_data + table_offset;

	if (rr_type == RR_MLC_HYNIX_20NM) {
		table_size = 1026;
		set_size = 64;
		set_offset = 2;
	} else if (rr_type == RR_MLC_HYNIX_1XNM) {
		table_size = 528;
		set_size = 32;
		set_offset = 16;
		index = 1;
		addr_1st[1] = 0x38;
		data[1] = 0x52;
	} else if (rr_type == RR_MLC_HYNIX_16NM) {
		table_size = 784;
		set_size = 48;
		set_offset = 16;
		index = 1;
		addr_1st[1] = 0x38;
		data[1] = 0x52;
		addr_2nd[2] = 0x1f;
	} else {
		NANDX_ASSERT(0);
	}

	table_offset += set_offset;
	table_data = mem_alloc(1, table_size);

	/* read retry table read start */
	nandx_chip_reset(chip);

	nfc->send_command(nfc, 0x36);
	for (; index < 2; index++) {
		nfc->send_address(nfc, addr_1st[index], 0, 1, 0);
		nfc->write_byte(nfc, data[index]);
	}

	for (index = 0; index < 5; index++)
		nfc->send_command(nfc, command[index]);
	for (index = 0; index < 5; index++)
		nfc->send_address(nfc, addr_2nd[index], 0, 1, 0);

	nfc->send_command(nfc, 0x30);
	for (index = 0; index < table_size; index++)
		table_data[index] = nfc->read_byte(nfc);

	nandx_chip_reset(chip);
	if (rr_type == RR_MLC_HYNIX_1XNM || rr_type == RR_MLC_HYNIX_16NM) {
		nfc->send_command(nfc, 0x36);
		nfc->send_address(nfc, 0x38, 0, 1, 0);
		nfc->write_byte(nfc, 0x00);
		nfc->send_command(nfc, 0x16);
		nfc->send_command(nfc, 0x00);
		nfc->send_address(nfc, 0x00, 0, 1, 0);
		nfc->send_command(nfc, 0x30);
	} else {
		nfc->send_command(nfc, 0x38);
	}

	/* read retry table check start */
	for (index = 0; index < 8; index++) {
		table_offset += set_size * index * 2;
		origin = table_data + table_offset;
		inverse = table_data + table_offset + set_size;
		for (count = 0; count < set_size; count++) {
			if ((origin[count] ^ inverse[count]) != 0xff)
				break;
		}
		if (count == set_size) {
			only_once = true;
			return table_data + table_offset;
		}
	}

	return NULL;
}

static void mlc_hynix_rr_get_parameters(struct nandx_chip *chip, u8 *param)
{
	u8 i, max_count;
	u8 rr_type = chip->dev_info->read_retry_type;
	u8 *mlc_hynix_rr_setting;

	mlc_hynix_rr_setting = mlc_hynix_rr_table_read(chip);
	NANDX_ASSERT(mlc_hynix_rr_setting);

	max_count = rr_type == RR_MLC_HYNIX_20NM ? 8 : 4;
	for (i = 0; i < max_count; i++)
		param[i] = mlc_hynix_rr_setting[chip->rr_count + i];

	chip->rr_count += max_count;
	max_count *= get_loop_count(chip);
	if (chip->rr_count >= max_count)
		chip->rr_count = 0;
}

static void mlc_hynix_rr_set_parameters(struct nandx_chip *chip, u8 *param)
{
	u8 i, max_count;
	u8 rr_type = chip->dev_info->read_retry_type;
	struct nfc_handler *nfc = chip->nfc;
	u8 address[8] = { 0xcc, 0xbf, 0xaa, 0xab, 0xcd, 0xad, 0xae, 0xaf };

	max_count = rr_type == RR_MLC_HYNIX_20NM ? 8 : 4;
	if (rr_type == RR_MLC_HYNIX_1XNM || rr_type == RR_MLC_HYNIX_16NM) {
		for (i = 0; i < max_count; i++)
			address[i] = 0x38 + i;
	}

	for (i = 0; i < max_count; i++) {
		nfc->send_command(nfc, 0x36);
		nfc->send_address(nfc, address[i], 0, 1, 0);
		nfc->write_byte(nfc, param[i]);
	}
	nfc->send_command(nfc, 0x16);
}

static void mlc_hynix_rr_exit(struct nandx_chip *chip)
{
	u8 param[RR_MAX_PARAM_NUM];

	chip->rr_count = 0;
	mlc_hynix_rr_get_parameters(chip, param);
	mlc_hynix_rr_set_parameters(chip, param);
	chip->rr_count = 0;
}

/* MLC HYNIX 20/1X/16NM READ RETRY End */

/* TLC SANDISK 1Y/1ZNM READ RETRY Start */
#define TLC_SANDISK_1YNM_SETS_NUM	(280)
#define SLC_SANDISK_1YNM_SETS_NUM	(11)
static void tlc_sandisk_1ynm_rr_get_parameters(struct nandx_chip *chip,
					       u8 *param)
{
	u16 i, max_count;
	u8 mlc_rr_setting[TLC_SANDISK_1YNM_SETS_NUM] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x04, 0x00, 0x00, 0x08, 0x00, 0x08, 0x04,
		0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x08,
		0x08, 0x04, 0x04, 0x04, 0x04, 0x04, 0x02,
		0x08, 0x04, 0x04, 0x08, 0x00, 0x08, 0x04,
		0x0c, 0x08, 0x04, 0x00, 0x00, 0x00, 0x08,
		0x10, 0x08, 0x04, 0x04, 0x00, 0x00, 0xfc,
		0x00, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
		0x00, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
		0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
		0x00, 0x12, 0x12, 0x12, 0x14, 0x14, 0x14,
		0xfc, 0x08, 0x08, 0x08, 0xfc, 0x08, 0x00,
		0xf8, 0xfc, 0x08, 0xfc, 0xf8, 0xfc, 0x04,
		0xf6, 0xfb, 0x00, 0x00, 0xf6, 0x00, 0xfc,
		0xf4, 0xfb, 0x08, 0x04, 0x04, 0x04, 0xfc,
		0xfa, 0xf8, 0xfc, 0xfe, 0x08, 0xfe, 0xfc,
		0xec, 0xf4, 0xf8, 0xfc, 0x00, 0xfc, 0xfc,
		0xec, 0xf8, 0xf8, 0xf8, 0xfa, 0xf8, 0xf8,
		0xe4, 0xfc, 0x02, 0x00, 0xf4, 0x00, 0x00,
		0xfe, 0xfe, 0xfe, 0xfc, 0x02, 0xfc, 0xfa,
		0xfd, 0x00, 0xfc, 0xff, 0x00, 0xff, 0xf8,
		0xfc, 0x00, 0xfb, 0xfe, 0xfe, 0xfd, 0xf6,
		0xfa, 0xfe, 0xfa, 0xfe, 0xfc, 0xfb, 0xf4,
		0xfa, 0xfd, 0xf9, 0xfd, 0xfa, 0xf9, 0xf2,
		0xfa, 0xfb, 0xf8, 0xfb, 0xf8, 0xf7, 0xf0,
		0xf8, 0xfa, 0xf7, 0xf9, 0xf6, 0xf5, 0xee,
		0xf4, 0xf9, 0xf6, 0xf8, 0xf4, 0xf3, 0xec,
		0xf2, 0xf8, 0xf4, 0xf5, 0xf2, 0xf1, 0xea,
		0xee, 0xf6, 0xf2, 0xf4, 0xee, 0xec, 0xe8,
		0xe8, 0xf4, 0xf0, 0xf0, 0xe8, 0xe4, 0xe0,
		0xe6, 0xf0, 0xec, 0xec, 0xe2, 0xe0, 0xda,
		0xfa, 0x00, 0x04, 0x02, 0x00, 0x00, 0x00,
		0xf8, 0xff, 0xfe, 0x00, 0xfe, 0xfe, 0xfe,
		0xf6, 0xfd, 0xfe, 0xfe, 0xfc, 0xfc, 0xfb,
		0xf4, 0xfc, 0xfd, 0xfd, 0xfa, 0xfa, 0xf9,
		0xf2, 0xfc, 0xfc, 0xfb, 0xf8, 0xf8, 0xf7,
		0xf0, 0xfb, 0xfb, 0xf9, 0xf6, 0xf6, 0xf5,
		0xee, 0xf9, 0xf9, 0xf8, 0xf4, 0xf4, 0xf3,
		0xed, 0xf8, 0xf8, 0xf6, 0xf2, 0xf2, 0xf1,
		0xea, 0xf6, 0xf7, 0xf4, 0xef, 0xf0, 0xef
	};
	u8 slc_rr_setting[SLC_SANDISK_1YNM_SETS_NUM] = {
		0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0xf0,
		0xe0, 0xd0, 0xc0
	};

	if (chip->slc_mode) {
		param[0] = slc_rr_setting[chip->rr_count];
		chip->rr_count += 1;
		max_count = SLC_SANDISK_1YNM_SETS_NUM;
	} else {
		for (i = 0; i < 7; i++)
			param[i] = mlc_rr_setting[chip->rr_count + i];
		chip->rr_count += 7;
		max_count = TLC_SANDISK_1YNM_SETS_NUM;
	}

	if (chip->rr_count >= max_count)
		chip->rr_count = 0;
}

#define TLC_SANDISK_1ZNM_SETS_NUM	(329)
#define SLC_SANDISK_1ZNM_SETS_NUM	(25)
static void tlc_sandisk_1znm_rr_get_parameters(struct nandx_chip *chip,
					       u8 *param)
{
	u16 i, max_count;
	u8 tlc_rr_setting[TLC_SANDISK_1ZNM_SETS_NUM] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xf8, 0x00, 0x10, 0x08, 0x08, 0x00, 0xf8,
		0xf8, 0xf8, 0xf0, 0xf8, 0xf8, 0xf8, 0xf8,
		0xe8, 0x00, 0x00, 0x00, 0xf8, 0xf0, 0xe0,
		0x00, 0x08, 0xf8, 0x08, 0x08, 0x00, 0x00,
		0xf0, 0xf8, 0xf8, 0xf8, 0x00, 0xf0, 0xe8,
		0xf0, 0x08, 0x08, 0xf8, 0xf0, 0xf0, 0x00,
		0x08, 0x08, 0x08, 0x08, 0xf8, 0xf8, 0xe8,
		0x00, 0xf0, 0x00, 0xf8, 0xf0, 0xf8, 0xf8,
		0x00, 0x08, 0x00, 0x00, 0xf8, 0x00, 0xe8,
		0xf8, 0x00, 0x00, 0xf8, 0xe8, 0xf8, 0x08,
		0xf8, 0x00, 0xf8, 0x08, 0xf0, 0xf8, 0xf8,
		0xf8, 0x08, 0x08, 0x00, 0x00, 0xf8, 0xf8,
		0x08, 0xf8, 0xf8, 0x00, 0x00, 0xf8, 0xf0,
		0xf0, 0xf8, 0x00, 0x00, 0xf8, 0x00, 0xf0,
		0x08, 0x00, 0x08, 0x00, 0xf0, 0xf8, 0xf0,
		0x10, 0x00, 0x10, 0x00, 0xf8, 0x08, 0x00,
		0xe8, 0x00, 0xf0, 0xf8, 0xf0, 0xf0, 0xf0,
		0xf0, 0x00, 0x08, 0xf8, 0xe8, 0x00, 0x08,
		0x08, 0x10, 0x10, 0x00, 0x08, 0xf8, 0x08,
		0x10, 0xf8, 0x18, 0x00, 0x08, 0xfc, 0x00,
		0x10, 0x08, 0x18, 0x08, 0x00, 0xfc, 0xf8,
		0x18, 0x10, 0x10, 0x00, 0x00, 0x00, 0xf0,
		0x00, 0x00, 0x04, 0x00, 0xfc, 0xfc, 0xfc,
		0x08, 0x08, 0x10, 0x00, 0x10, 0xfc, 0x10,
		0x00, 0x10, 0x08, 0x00, 0x10, 0xfc, 0x10,
		0xf8, 0x00, 0x00, 0xf8, 0x10, 0xfc, 0x10,
		0xf0, 0xf8, 0xf8, 0xf8, 0x08, 0xfc, 0x08,
		0xe8, 0x04, 0xf0, 0x00, 0x08, 0x00, 0x00,
		0xe8, 0x08, 0xf0, 0x08, 0x00, 0x08, 0xe8,
		0xe0, 0x10, 0xe8, 0x08, 0xf8, 0x08, 0xe8,
		0xe0, 0x08, 0x00, 0x10, 0x00, 0x10, 0x00,
		0xe0, 0x04, 0x00, 0xf8, 0xf0, 0xf8, 0x00,
		0xe0, 0x04, 0x00, 0x08, 0xe8, 0x08, 0x00,
		0xe0, 0x04, 0x00, 0x00, 0xe0, 0xfc, 0x00,
		0xe8, 0x04, 0x00, 0x10, 0xe8, 0x10, 0x00,
		0xe8, 0xfc, 0x00, 0x08, 0xe0, 0x08, 0x00,
		0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0xfc, 0x00, 0x00, 0x00, 0xfc, 0x00,
		0x00, 0xfc, 0x00, 0xf8, 0x00, 0xf8, 0x00,
		0x00, 0xfc, 0x00, 0xf0, 0x00, 0xf0, 0x00,
		0x00, 0xf0, 0x00, 0xf0, 0x00, 0xf0, 0x00,
		0x00, 0xf8, 0x00, 0xf0, 0x00, 0xf0, 0x00,
		0x00, 0xf0, 0x00, 0xe8, 0x00, 0xe8, 0x00,
		0x00, 0xe8, 0x00, 0xe8, 0x00, 0xe8, 0x00,
		0x00, 0xe8, 0x00, 0xf0, 0x00, 0xf0, 0x00,
		0x00, 0xe8, 0x00, 0xe0, 0x00, 0xe0, 0x00
	};
	u8 slc_rr_setting[SLC_SANDISK_1ZNM_SETS_NUM] = {
		0x00, 0x08, 0xf8, 0x10, 0xf0, 0x18, 0xe8, 0x20, 0xe0, 0x28,
		0xd8, 0x30, 0xd0, 0x38, 0xc8, 0x40, 0xc0, 0x48, 0xb8, 0x50,
		0xb0, 0x58, 0x60, 0x68, 0x70
	};

	if (chip->slc_mode) {
		param[0] = slc_rr_setting[chip->rr_count];
		chip->rr_count += 1;
		max_count = SLC_SANDISK_1ZNM_SETS_NUM;
	} else {
		for (i = 0; i < 7; i++)
			param[i] = tlc_rr_setting[chip->rr_count + i];
		chip->rr_count += 7;
		max_count = TLC_SANDISK_1ZNM_SETS_NUM;
	}

	if (chip->rr_count >= max_count)
		chip->rr_count = 0;
}

static void tlc_sandisk_rr_set_parameters(struct nandx_chip *chip, u8 *param)
{
	int ret;
	u8 back[RR_MAX_PARAM_NUM] = { 0 };

	if (chip->slc_mode) {
		ret = nandx_chip_set_feature_with_check(chip, 0x14, param,
							back, RR_FEATURE_NUM);
		NANDX_ASSERT(!ret);
	} else {
		ret = nandx_chip_set_feature_with_check(chip, 0x12, param,
							back, RR_FEATURE_NUM);
		NANDX_ASSERT(!ret);
		ret = nandx_chip_set_feature_with_check(chip, 0x13,
							param +
							RR_FEATURE_NUM, back,
							RR_FEATURE_NUM);
		NANDX_ASSERT(!ret);
	}
}

static void tlc_sandisk_rr_exit(struct nandx_chip *chip)
{
	struct nfc_handler *nfc = chip->nfc;

	chip->rr_count = 0;

	nfc->send_command(nfc, 0x55);
	nfc->send_address(nfc, 0x00, 0, 1, 0);
	nfc->write_byte(nfc, 0x00);

	nandx_chip_reset(chip);
}

/* TLC SANDISK 1Y/1ZNM READ RETRY End */

/* TLC TOSHIBA 15NM READ RETRY Start */
#define TLC_TOSHIBA_15NM_SETS_NUM	(217)
#define SLC_TOSHIBA_15NM_SETS_NUM	(7)
static void tlc_toshiba_15nm_rr_get_parameters(struct nandx_chip *chip,
					       u8 *param)
{
	u16 i, max_count;
	u8 tlc_rr_setting[TLC_TOSHIBA_15NM_SETS_NUM] = {
		0xfe, 0x03, 0x02, 0x02, 0xff, 0xfc, 0xfd,
		0xfe, 0x02, 0x01, 0x01, 0xfe, 0xfa, 0xfb,
		0xfe, 0x00, 0x00, 0xff, 0xfc, 0xf8, 0xf9,
		0xfd, 0xff, 0xfe, 0xfe, 0xfa, 0xf6, 0xf7,
		0xfd, 0xfe, 0xfd, 0xfc, 0xf8, 0xf4, 0xf5,
		0xfd, 0xfd, 0xfc, 0xfb, 0xf6, 0xf2, 0xf2,
		0xfd, 0xfb, 0xfb, 0xf9, 0xf5, 0xf0, 0xf0,
		0xfd, 0xfa, 0xf9, 0xf8, 0xf3, 0xee, 0xee,
		0xfd, 0xf9, 0xf8, 0xf6, 0xf1, 0xec, 0xec,
		0xfd, 0xf8, 0xf7, 0xf5, 0xef, 0xea, 0xe9,
		0xfa, 0xfa, 0xfb, 0xfa, 0xfb, 0xfa, 0xfa,
		0xfa, 0xfa, 0xfa, 0xf9, 0xfa, 0xf8, 0xf8,
		0xfa, 0xfa, 0xfa, 0xf8, 0xf9, 0xf6, 0xf5,
		0xfb, 0xfa, 0xf9, 0xf7, 0xf7, 0xf4, 0xf3,
		0xfb, 0xfb, 0xf9, 0xf6, 0xf6, 0xf2, 0xf0,
		0xfb, 0xfb, 0xf8, 0xf5, 0xf5, 0xf0, 0xee,
		0xfb, 0xfb, 0xf8, 0xf5, 0xf4, 0xee, 0xeb,
		0xfc, 0xfb, 0xf7, 0xf4, 0xf2, 0xec, 0xe9,
		0xf5, 0xfd, 0xfd, 0xff, 0xfc, 0xfa, 0xfc,
		0xf5, 0xfb, 0xfb, 0xfc, 0xf9, 0xf6, 0xf9,
		0xf5, 0xfa, 0xfa, 0xfa, 0xf8, 0xf4, 0xf7,
		0xfe, 0x03, 0x03, 0x04, 0x01, 0xff, 0x01,
		0xfc, 0x00, 0x00, 0x01, 0xfe, 0xfc, 0xfe,
		0xfa, 0xfa, 0xfc, 0xfc, 0xfa, 0xf7, 0xfa,
		0x00, 0x03, 0x02, 0x03, 0xff, 0xfc, 0xfe,
		0x04, 0x03, 0x03, 0x03, 0x00, 0xfc, 0xfd,
		0x08, 0x04, 0x03, 0x04, 0x00, 0xfc, 0xfc,
		0xf7, 0x03, 0x03, 0x06, 0x04, 0x04, 0x08,
		0xfa, 0x04, 0x06, 0x09, 0x09, 0x08, 0x0c,
		0xfc, 0x06, 0x09, 0x0c, 0x0d, 0x0c, 0x10,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	u8 slc_rr_setting[SLC_TOSHIBA_15NM_SETS_NUM] = {
		0xf0, 0xe0, 0xd0, 0xc0, 0x20, 0x30, 0x40
	};

	if (chip->slc_mode) {
		param[0] = slc_rr_setting[chip->rr_count];
		chip->rr_count += 1;
		max_count = SLC_TOSHIBA_15NM_SETS_NUM;
	} else {
		for (i = 0; i < 7; i++)
			param[i] = tlc_rr_setting[chip->rr_count + i];
		chip->rr_count += 7;
		max_count = TLC_TOSHIBA_15NM_SETS_NUM;
	}

	if (chip->rr_count >= max_count)
		chip->rr_count = 0;
}

static void tlc_toshiba_rr_set_parameters(struct nandx_chip *chip, u8 *param)
{
	u8 back[RR_MAX_PARAM_NUM] = { 0 };

	/* LUN address: LUN0 = 0x00, LUN1 = 0x01, LUN2 = 0x02 ... */
	if (chip->slc_mode) {
		nandx_chip_set_lun_feature_with_check(chip, 0x00, 0x14, param,
						      back, RR_FEATURE_NUM);
	} else {
		nandx_chip_set_lun_feature_with_check(chip, 0x00, 0x12, param,
						      back, RR_FEATURE_NUM);
		nandx_chip_set_lun_feature_with_check(chip, 0x00, 0x13,
						      param + RR_FEATURE_NUM,
						      back, RR_FEATURE_NUM);
	}
}

static void tlc_toshiba_rr_exit(struct nandx_chip *chip)
{
	u8 param[RR_MAX_PARAM_NUM] = { 0 };
	u8 back[RR_MAX_PARAM_NUM] = { 0 };

	/* LUN address: LUN0 = 0x00, LUN1 = 0x01, LUN2 = 0x02 ... */
	if (chip->slc_mode) {
		nandx_chip_set_lun_feature_with_check(chip, 0x00, 0x14, param,
						      back, RR_FEATURE_NUM);
	} else {
		nandx_chip_set_lun_feature_with_check(chip, 0x00, 0x12, param,
						      back, RR_FEATURE_NUM);
		nandx_chip_set_lun_feature_with_check(chip, 0x00, 0x13, param,
						      back, RR_FEATURE_NUM);
	}

	chip->rr_count = 0;
}

/* TLC TOSHIBA 15NM READ RETRY End */

static struct read_retry_ops nand_read_retry[RR_TYPE_NUM] = {
	{0, 0, (rr_entry_t)rr_none, (rr_get_parameters_t)rr_none,
	 (rr_set_parameters_t)rr_none, (rr_enable_t)rr_none,
	 (rr_exit_t)rr_none},	/* RR_TYPE_NONE */

	{8, 8, (rr_entry_t)rr_none, micron_rr_get_parameters,
	 micron_rr_set_parameters, (rr_enable_t)rr_none,
	 micron_rr_exit},	/* RR_MLC_MICRON */

	{10, 10, (rr_entry_t)rr_none, l95b_rr_get_parameters,
	 micron_rr_set_parameters, (rr_enable_t)rr_none,
	 micron_rr_exit},	/* RR_MLC_MICRON_L95B */

	{37, 23, (rr_entry_t)rr_none, mlc_sandisk_rr_get_parameters,
	 mlc_sandisk_rr_set_parameters, mlc_sandisk_rr_enable,
	 mlc_sandisk_rr_exit},	/* RR_MLC_SANDISK */

	{21, 17, mlc_sandisk_19nm_rr_entry,
	 mlc_sandisk_19nm_rr_get_parameters,
	 mlc_sandisk_19nm_rr_set_parameters, mlc_sandisk_19nm_rr_enable,
	 mlc_sandisk_19nm_rr_exit},	/* RR_MLC_SANDISK_19NM */

	{32, 25, (rr_entry_t)rr_none, mlc_sandisk_1znm_rr_get_parameters,
	 mlc_sandisk_rr_set_parameters, sandisk_rr_enable,
	 mlc_sandisk_rr_exit},	/* RR_MLC_SANDISK_1ZNM */

	{7, 7, mlc_toshiba_19nm_rr_entry, mlc_toshiba_19nm_rr_get_parameters,
	 mlc_toshiba_19nm_rr_set_parameters, mlc_toshiba_19nm_rr_enable,
	 mlc_toshiba_19nm_rr_exit},	/* RR_MLC_TOSHIBA_19NM */

	{10, 10, mlc_toshiba_19nm_rr_entry,
	 mlc_toshiba_15nm_rr_get_parameters,
	 mlc_toshiba_19nm_rr_set_parameters, mlc_toshiba_15nm_rr_enable,
	 mlc_toshiba_19nm_rr_exit},	/* RR_MLC_TOSHIBA_15NM */

	{8, 8, (rr_entry_t)rr_none, mlc_hynix_rr_get_parameters,
	 mlc_hynix_rr_set_parameters, (rr_enable_t)rr_none,
	 mlc_hynix_rr_exit},	/* RR_MLC_HYNIX_20NM */

	{8, 8, (rr_entry_t)rr_none, mlc_hynix_rr_get_parameters,
	 mlc_hynix_rr_set_parameters, (rr_enable_t)rr_none,
	 mlc_hynix_rr_exit},	/* RR_MLC_HYNIX_1XNM */

	{12, 12, (rr_entry_t)rr_none, mlc_hynix_rr_get_parameters,
	 mlc_hynix_rr_set_parameters, (rr_enable_t)rr_none,
	 mlc_hynix_rr_exit},	/* RR_MLC_HYNIX_16NM */

	{40, 11, (rr_entry_t)rr_none, tlc_sandisk_1ynm_rr_get_parameters,
	 tlc_sandisk_rr_set_parameters, sandisk_rr_enable,
	 tlc_sandisk_rr_exit},	/* RR_TLC_SANDISK_1YNM */

	{47, 25, (rr_entry_t)rr_none, tlc_sandisk_1znm_rr_get_parameters,
	 tlc_sandisk_rr_set_parameters, sandisk_rr_enable,
	 tlc_sandisk_rr_exit},	/* RR_TLC_SANDISK_1ZNM */

	{31, 7, (rr_entry_t)rr_none, tlc_toshiba_15nm_rr_get_parameters,
	 tlc_toshiba_rr_set_parameters, (rr_enable_t)rr_none,
	 tlc_toshiba_rr_exit},	/* RR_TLC_TOSHIBA_15NM */
};

struct read_retry_ops *get_read_retry_ops(struct nandx_device_info *dev_info)
{
	u64 total_size;
	struct read_retry_ops *rr_ops;

	rr_ops = &nand_read_retry[dev_info->read_retry_type];

	/* check read retry loop count */
	switch (dev_info->read_retry_type) {
	case RR_MLC_MICRON:
		if (dev_info->type == NAND_MLC)
			break;

		rr_ops->loop_count = 16;
		rr_ops->slc_loop_count = rr_ops->loop_count;
		break;

	case RR_MLC_SANDISK:
		/* dsp off, dsp on, cmd25 dsp on, cmd25 dsp off */
		rr_ops->loop_count = 37 * 4;
		rr_ops->slc_loop_count = 23;
		break;

	case RR_MLC_SANDISK_1ZNM:
		total_size = (u64)dev_info->block_num * dev_info->block_size;
		if ((u32)(total_size >> 10) > MB(8))
			rr_ops->loop_count = 33;
		break;

	default:
		break;
	}

	return rr_ops;
}

int setup_read_retry(struct nandx_chip *chip, int count)
{
	u16 rr_loop_count;
	u8 param[RR_MAX_PARAM_NUM] = { 0 };
	struct read_retry_ops *rr_ops = chip->rr_ops;

	rr_loop_count = get_loop_count(chip);

	if (count == -1) {
		rr_ops->exit(chip);
		return 0;
	}

	if (count >= rr_loop_count)
		return -ENANDREAD;

	if (count == 0)
		rr_ops->entry(chip);

	rr_ops->get_parameters(chip, param);
	rr_ops->set_parameters(chip, param);
	rr_ops->enable(chip);

	return 0;
}

int nandx_chip_read_retry(struct nandx_chip *chip,
			  struct nandx_ops *ops, int *count)
{
	int ret = 0, num;
	u16 rr_loop_count;
	u8 param[RR_MAX_PARAM_NUM] = { 0 };
	struct read_retry_ops *rr_ops = chip->rr_ops;

	if (!rr_ops) {
		ops->status = -ENANDREAD;
		return -ENANDREAD;
	}

	rr_loop_count = get_loop_count(chip);
	num = ops->len / chip->nfc->sector_size;

	rr_ops->entry(chip);

	for (*count = 0; *count < rr_loop_count; (*count)++) {
		rr_ops->get_parameters(chip, param);
		rr_ops->set_parameters(chip, param);
		rr_ops->enable(chip);

		if (chip->slc_mode)
			slc_mode_entry(chip, false);

		nandx_chip_read_page(chip, ops->row);

		if (chip->randomize)
			nandx_chip_enable_randomizer(chip, ops->row, false);

		nandx_chip_random_output(chip, ops->row, ops->col);
		ret = nandx_chip_read_data(chip, num, ops->data, ops->oob);

		if (chip->randomize)
			nandx_chip_disable_randomizer(chip);

		if (ret != -ENANDREAD)
			break;
	}

	rr_ops->exit(chip);

	pr_info("nandx-rr2 %d %d\n", *count, ret);

	ops->status = ret;
	return ret;
}

/* auto calibration */
static int micron_auto_calibration(struct nandx_chip *chip, int count)
{
	int ret, level = 20;
	u8 param[4] = { 0x00 }, back[4] = { 0xff };

	/* enable micron auto calibration read */
	if (count == 0) {
		param[0] = 0x01;
		ret = nandx_chip_set_feature_with_check(chip, 0x96,
							param, back, 4);
		if (ret)
			return 0;
	}

	/* change nand drive strength */
	if (count > level - DRIVE_LEVEL_NUM && count <= level) {
		ret = nandx_chip_calibration(chip);
		if (ret)
			count = level + 1;
	}

	/* exit micron auto calibration read */
	if (count > level || count < 0) {
		param[0] = 0x00;
		ret = nandx_chip_set_feature_with_check(chip, 0x96,
							param, back, 4);
		return 0;
	}

	return 1;
}

int nandx_chip_auto_calibration(struct nandx_chip *chip, int count)
{
	switch (chip->vendor_type) {
	case VENDOR_MICRON:
		return micron_auto_calibration(chip, count);

	default:
		break;
	}

	return 0;
}
