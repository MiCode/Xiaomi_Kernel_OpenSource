// SPDX-License-Identifier: GPL-2.0
/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
 *
 * Copyright (C) 2022 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*****************************************************************************
 *
 * File Name: focaltech_point_report_check.c
 *
 * Author: Focaltech Driver Team
 *
 * Created: 2016-11-16
 *
 * Abstract: point report check function
 *
 * Version: v1.0
 *
 * Revision History:
 *
 *****************************************************************************/

/*****************************************************************************
 * Included header files
 *****************************************************************************/
#include "focaltech_core.h"

/*****************************************************************************
 * Private constant and macro definitions using #define
 *****************************************************************************/
#define POINT_REPORT_CHECK_WAIT_TIME                200    /* unit:ms */
#define PRC_INTR_INTERVALS                          100    /* unit:ms */

/*****************************************************************************
 * Static variables
 *****************************************************************************/

/*****************************************************************************
 * functions body
 *****************************************************************************/
/*****************************************************************************
 *  Name: fts_prc_func
 *  Brief: fts point report check work func, report whole up of points
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
static void fts_prc_func(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work,
								  struct fts_ts_data, prc_work.work);
	unsigned long cur_jiffies = jiffies;
	unsigned long intr_timeout = msecs_to_jiffies(PRC_INTR_INTERVALS);

	if (ts_data->prc_support && !ts_data->suspended) {
		intr_timeout += ts_data->intr_jiffies;
		if (time_after(cur_jiffies, intr_timeout)) {
			if (ts_data->touch_points) {
				fts_release_all_finger();
				if (ts_data->log_level >= 3)
					FTS_DEBUG("prc trigger interval:%dms",
							  jiffies_to_msecs(cur_jiffies - ts_data->intr_jiffies));
			}
			ts_data->prc_mode = 0;
		} else {
			queue_delayed_work(ts_data->ts_workqueue, &ts_data->prc_work,
							   msecs_to_jiffies(POINT_REPORT_CHECK_WAIT_TIME));
			ts_data->prc_mode = 1;
		}
	} else {
		ts_data->prc_mode = 0;
	}
}

/*****************************************************************************
 *  Name: fts_prc_queue_work
 *  Brief: fts point report check queue work, call it when interrupt comes
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
void fts_prc_queue_work(struct fts_ts_data *ts_data)
{
	if (ts_data->prc_support && !ts_data->prc_mode && !ts_data->suspended) {
		queue_delayed_work(ts_data->ts_workqueue, &ts_data->prc_work,
						   msecs_to_jiffies(POINT_REPORT_CHECK_WAIT_TIME));
		ts_data->prc_mode = 1;
	}
}


static ssize_t fts_prc_store(
	struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf)) {
		FTS_DEBUG("enable prc");
		ts_data->prc_support = ENABLE;
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		FTS_DEBUG("disable prc");
		cancel_delayed_work_sync(&ts_data->prc_work);
		ts_data->prc_support = DISABLE;
	}
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_prc_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	count = snprintf(buf, PAGE_SIZE, "PRC: %s\n",
			ts_data->prc_support ? "Enable" : "Disable");
	mutex_unlock(&input_dev->mutex);

	return count;
}

static DEVICE_ATTR_RW(fts_prc);

/*****************************************************************************
 *  Name: fts_point_report_check_init
 *  Brief:
 *  Input:
 *  Output:
 *  Return: < 0: Fail to create esd check queue
 *****************************************************************************/
int fts_point_report_check_init(struct fts_ts_data *ts_data)
{
	int ret = 0;

	FTS_FUNC_ENTER();

	if (ts_data->ts_workqueue)
		INIT_DELAYED_WORK(&ts_data->prc_work, fts_prc_func);
	else {
		FTS_ERROR("fts workqueue is NULL, can't run point report check function");
		return -EINVAL;
	}

	ret = sysfs_create_file(&ts_data->dev->kobj, &dev_attr_fts_prc.attr);
	if (ret < 0)
		FTS_ERROR("create prc sysfs fail");

	ts_data->prc_support = FTS_POINT_REPORT_CHECK_EN;
	FTS_FUNC_EXIT();
	return 0;
}

/*****************************************************************************
 *  Name: fts_point_report_check_exit
 *  Brief:
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
int fts_point_report_check_exit(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	cancel_delayed_work_sync(&ts_data->prc_work);
	sysfs_remove_file(&ts_data->dev->kobj, &dev_attr_fts_prc.attr);
	FTS_FUNC_EXIT();
	return 0;
}
