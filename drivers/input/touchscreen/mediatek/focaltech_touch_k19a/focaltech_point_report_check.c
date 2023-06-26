/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
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

#if FTS_POINT_REPORT_CHECK_EN
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define POINT_REPORT_CHECK_WAIT_TIME				200	/* unit:ms */
#define PRC_INTR_INTERVALS						  100	/* unit:ms */

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

	intr_timeout += ts_data->intr_jiffies;
	if (time_after(cur_jiffies, intr_timeout)) {
		fts_release_all_finger();
		ts_data->prc_mode = 0;
		//FTS_DEBUG("interval:%lu", (cur_jiffies - ts_data->intr_jiffies) * 1000 / HZ);
	} else {
		queue_delayed_work(ts_data->ts_workqueue, &ts_data->prc_work,
						   msecs_to_jiffies(POINT_REPORT_CHECK_WAIT_TIME));
		ts_data->prc_mode = 1;
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
	ts_data->intr_jiffies = jiffies;
	if (!ts_data->prc_mode) {
		queue_delayed_work(ts_data->ts_workqueue, &ts_data->prc_work,
						   msecs_to_jiffies(POINT_REPORT_CHECK_WAIT_TIME));
		ts_data->prc_mode = 1;
	}
}

/*****************************************************************************
*  Name: fts_point_report_check_init
*  Brief:
*  Input:
*  Output:
*  Return: < 0: Fail to create esd check queue
*****************************************************************************/
int fts_point_report_check_init(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();

	if (ts_data->ts_workqueue) {
		INIT_DELAYED_WORK(&ts_data->prc_work, fts_prc_func);
	} else {
		FTS_ERROR("fts workqueue is NULL, can't run point report check function");
		return -EINVAL;
	}

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

	FTS_FUNC_EXIT();
	return 0;
}
#endif /* FTS_POINT_REPORT_CHECK_EN */

