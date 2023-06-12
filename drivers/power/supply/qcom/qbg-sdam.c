// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/regmap.h>

#include <uapi/linux/qbg.h>

#include "qbg-core.h"
#include "qbg-sdam.h"

int qbg_sdam_read(struct qti_qbg *chip, int offset, u8 *data, int len)
{
	int rc;

	rc = regmap_bulk_read(chip->regmap, offset, data, len);
	if (rc < 0)
		pr_err("Failed to read QBG SDAM offset:%04x rc=%d\n",
			offset, rc);

	return rc;
}

int qbg_sdam_write(struct qti_qbg *chip, int offset, u8 *data, int len)
{
	int rc;

	rc = regmap_bulk_write(chip->regmap, offset, data, len);
	if (rc < 0)
		pr_err("Failed to write QBG SDAM offset:%04x rc=%d\n",
			offset, rc);

	return rc;
}

int qbg_sdam_get_fifo_data(struct qti_qbg *chip, struct fifo_data *fifo,
				u32 fifo_count)
{
	int rc, i;
	u32 num_sdams, fifo_data_length, bytes_read = 0, bytes_remaining;

	fifo_data_length = fifo_count * QBG_ONE_FIFO_LENGTH;
	num_sdams = fifo_data_length / QBG_SDAM_ONE_FIFO_REGION_SIZE;

	if (num_sdams > chip->num_data_sdams) {
		pr_err("Fifo count exceeds QBG SDAMs\n");
		return -EINVAL;
	}

	qbg_dbg(chip, QBG_DEBUG_SDAM, "Completely filled sdams=%d\n",
		num_sdams);

	/* First read SDAMs that are completely filled */
	for (i = 0; i < num_sdams; i++) {
		rc = qbg_sdam_read(chip,
			QBG_SDAM_DATA_START_OFFSET(chip, (SDAM_DATA0 + i)),
			(((u8 *)fifo) + bytes_read),
			QBG_SDAM_ONE_FIFO_REGION_SIZE);
		if (rc < 0) {
			pr_err("Failed to read QBG SDAM%d, rc=%d\n", i, rc);
			return rc;
		}
		bytes_read += QBG_SDAM_ONE_FIFO_REGION_SIZE;

	}

	/* Next read SDAM that is partially filled */
	bytes_remaining = fifo_data_length - bytes_read;
	qbg_dbg(chip, QBG_DEBUG_SDAM, "Valid data in partially filled sdam:%d bytes\n",
		bytes_remaining);

	if (bytes_remaining) {
		rc = qbg_sdam_read(chip,
				QBG_SDAM_DATA_START_OFFSET(chip, (SDAM_DATA0 + i)),
				(((u8 *)fifo) + bytes_read),
				bytes_remaining);
		if (rc < 0) {
			pr_err("Failed to read remaining bytes from QBG SDAM%d, rc=%d\n",
				i, rc);
			return rc;
		}
		bytes_read += bytes_remaining;
	}

	qbg_dbg(chip, QBG_DEBUG_SDAM, "Total bytes read=%d\n", bytes_read);

	return rc;
}

int qbg_sdam_get_essential_params(struct qti_qbg *chip, u8 *params)
{
	int rc;

	if (!chip || !params)
		return -EINVAL;

	rc = qbg_sdam_read(chip,
			chip->sdam_base + (SDAM_CTRL1 * QBG_SINGLE_SDAM_SIZE) +
			QBG_ESSENTIAL_PARAMS_START_OFFSET,
			(u8 *)params, sizeof(struct qbg_essential_params));
	if (rc < 0)
		pr_err("Failed to read QBG essential params, rc=%d\n", rc);
	else
		qbg_dbg(chip, QBG_DEBUG_SDAM, "Read QBG essential params\n");

	return rc;
}

int qbg_sdam_set_essential_params(struct qti_qbg *chip, u8 *params)
{
	int rc;

	if (!chip || !params)
		return -EINVAL;

	rc = qbg_sdam_write(chip,
			chip->sdam_base + (SDAM_CTRL1 * QBG_SINGLE_SDAM_SIZE) +
			QBG_ESSENTIAL_PARAMS_START_OFFSET,
			(u8 *)params, sizeof(struct qbg_essential_params));
	if (rc < 0)
		pr_err("Failed to write QBG essential params, rc=%d\n", rc);
	else
		qbg_dbg(chip, QBG_DEBUG_SDAM, "Written QBG essential params\n");

	return rc;
}

int qbg_sdam_get_essential_param_revid(struct qti_qbg *chip, u8 *revid)
{
	int rc;

	if (!chip || !revid)
		return -EINVAL;

	rc = qbg_sdam_read(chip,
			chip->sdam_base + (SDAM_CTRL1 * QBG_SINGLE_SDAM_SIZE) +
			QBG_ESSENTIAL_PARAMS_REVID_OFFSET,
			revid, 1);
	if (rc < 0)
		pr_err("Failed to read QBG essential params revid, rc=%d\n", rc);
	else
		qbg_dbg(chip, QBG_DEBUG_SDAM, "Read QBG essential params revid:%u\n", *revid);

	return rc;
}

int qbg_sdam_set_essential_param_revid(struct qti_qbg *chip, u8 revid)
{
	int rc;

	if (!chip)
		return -EINVAL;

	rc = qbg_sdam_write(chip,
			chip->sdam_base + (SDAM_CTRL1 * QBG_SINGLE_SDAM_SIZE) +
			QBG_ESSENTIAL_PARAMS_REVID_OFFSET,
			&revid, 1);
	if (rc < 0)
		pr_err("Failed to write QBG essential params revid, rc=%d\n", rc);
	else
		qbg_dbg(chip, QBG_DEBUG_SDAM, "Store QBG essential params revid:%u\n",
				revid);

	return rc;
}

int qbg_sdam_get_battery_id(struct qti_qbg *chip, u32 *battid)
{
	int rc;
	u8 buf[2];

	if (!chip || !battid)
		return -EINVAL;

	rc = qbg_sdam_read(chip,
			chip->sdam_base + (SDAM_CTRL1 * QBG_SINGLE_SDAM_SIZE) +
			QBG_ESSENTIAL_PARAMS_BATTID_OFFSET,
			buf, 2);
	if (rc < 0) {
		pr_err("Failed to read QBG battery ID, rc=%d\n", rc);
	} else {
		*battid = buf[0] | (buf[1] << 8);
		qbg_dbg(chip, QBG_DEBUG_SDAM, "Read QBG battery ID:%u\n",
			*battid);
	}

	return rc;
}

int qbg_sdam_set_battery_id(struct qti_qbg *chip, u32 battid)
{
	int rc;

	if (!chip)
		return -EINVAL;

	rc = qbg_sdam_write(chip,
			chip->sdam_base + (SDAM_CTRL1 * QBG_SINGLE_SDAM_SIZE) +
			QBG_ESSENTIAL_PARAMS_BATTID_OFFSET,
			(u8 *)&battid, 2);
	if (rc < 0)
		pr_err("Failed to write QBG battery ID, rc=%d\n", rc);
	else
		qbg_dbg(chip, QBG_DEBUG_SDAM, "Store QBG battery ID:%u\n", battid);

	return rc;
}
