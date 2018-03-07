/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2016, FocalTech Systems, Ltd., all rights reserved.
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
*    Author: WangTao
*
*   Created: 2016-11-16
*
*  Abstract: point report check function
*
*   Version: v1.0
*
* Revision History:
*        v1.0:
*            First release. By WangTao 2016-11-16
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "focaltech_core.h"

#if FTS_POINT_REPORT_CHECK_EN
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define POINT_REPORT_CHECK_WAIT_TIME              200

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/

/*****************************************************************************
* Static variables
*****************************************************************************/
static struct delayed_work fts_point_report_check_work;
static struct workqueue_struct *fts_point_report_check_workqueue;

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

/*****************************************************************************
* functions body
*****************************************************************************/


/*****************************************************************************
*  Name: fts_point_report_check_func
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void fts_point_report_check_func(struct work_struct *work)
{
int i;
#if FTS_MT_PROTOCOL_B_EN
	unsigned int finger_count = 0;

	FTS_FUNC_ENTER();

	for (finger_count = 0; finger_count < FTS_MAX_POINTS; finger_count++) {
		input_mt_slot(fts_input_dev, finger_count);
		input_mt_report_slot_state(fts_input_dev, MT_TOOL_FINGER, false);
	}
#else
	input_mt_sync(fts_input_dev);
#endif
	if (fts_wq_data->pdata->have_key) {
		printk("point_num 1 = %d\n", (fts_wq_data->event).point_num);

		FTS_DEBUG("Keys All Up!");
		for (i = 0; i < fts_wq_data->pdata->key_number; i++) {
			input_report_key(fts_wq_data->input_dev, fts_wq_data->pdata->keys[i], 0);
		}

	}
	input_report_key(fts_input_dev, BTN_TOUCH, 0);
	input_sync(fts_input_dev);



	FTS_FUNC_EXIT();
}

void fts_point_report_check_queue_work(void)
{

	cancel_delayed_work(&fts_point_report_check_work);
	queue_delayed_work(fts_point_report_check_workqueue, &fts_point_report_check_work, msecs_to_jiffies(POINT_REPORT_CHECK_WAIT_TIME));

}

/*****************************************************************************
*  Name: fts_point_report_check_init
*  Brief:
*  Input:
*  Output:
*  Return: < 0: Fail to create esd check queue
*****************************************************************************/
int fts_point_report_check_init(void)
{
	FTS_FUNC_ENTER();

	INIT_DELAYED_WORK(&fts_point_report_check_work, fts_point_report_check_func);
	fts_point_report_check_workqueue = create_workqueue("fts_point_report_check_func_wq");
	if (fts_point_report_check_workqueue == NULL) {
		FTS_ERROR("[POINT_REPORT]: Failed to create fts_point_report_check_workqueue!!");
	} else {
		FTS_DEBUG("[POINT_REPORT]: Success to create fts_point_report_check_workqueue!!");
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
int fts_point_report_check_exit(void)
{
	FTS_FUNC_ENTER();

	destroy_workqueue(fts_point_report_check_workqueue);

	FTS_FUNC_EXIT();
	return 0;
}
#endif /* FTS_POINT_REPORT_CHECK_EN */

