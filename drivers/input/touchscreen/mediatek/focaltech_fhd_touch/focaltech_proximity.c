/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

/*****************************************************************************
 *
 * File Name: focaltech_proximity.c
 *
 *    Author: Focaltech Driver Team
 *
 *   Created: 2016-09-19
 *
 *  Abstract: close proximity function
 *
 *   Version: v1.0
 *
 * Revision History:
 *        v1.0:
 *            First release based on xiaguobin's solution. By luougojin
 *2016-08-19
 *****************************************************************************/

/*****************************************************************************
 * Included header files
 *****************************************************************************/
#include "focaltech_common.h"
#include "focaltech_core.h"

#if FTS_PSENSOR_EN
#include <alsps.h>
#include <hwmsen_dev.h>
#include <hwmsensor.h>
#include <sensors_io.h>

/*****************************************************************************
 * Private constant and macro definitions using #define
 *****************************************************************************/
/*
 * FTS_ALSPS_SUPPORT is choose structure hwmsen_object or control_path,
 * data_path
 * FTS_ALSPS_SUPPORT = 1, is control_path, data_path
 * FTS_ALSPS_SUPPORT = 0, hwmsen_object
 */
#define FTS_ALSPS_SUPPORT 1
#define PS_FAR_AWAY 1
#define PS_NEAR 0

/*****************************************************************************
 * Private enumerations, structures and unions using typedef
 *****************************************************************************/
struct fts_proximity_st {
	u8 mode : 1;   /* 1- proximity enable 0- disable */
	u8 detect : 1; /* 0-->close ; 1--> far away */
	u8 unused : 4;
};

/*****************************************************************************
 * Static variables
 *****************************************************************************/
static struct fts_proximity_st fts_proximity_data;

/*****************************************************************************
 * Global variable or extern global variabls/functions
 *****************************************************************************/

/*****************************************************************************
 * Static function prototypes
 *****************************************************************************/

/************************************************************************
 * Name: fts_enter_proximity_mode
 * Brief:  change proximity mode
 * Input:  proximity mode
 * Output: no
 * Return: success =0
 ***********************************************************************/
static int fts_enter_proximity_mode(int mode)
{
	int ret = 0;
	u8 buf_addr = 0;
	u8 buf_value = 0;

	buf_addr = FTS_REG_FACE_DEC_MODE_EN;
	if (mode)
		buf_value = 0x01;
	else
		buf_value = 0x00;

	ret = fts_write_reg(buf_addr, buf_value);
	if (ret < 0) {
		FTS_ERROR("[PROXIMITY] Write proximity register(0xB0) fail!");
		return ret;
	}

	fts_proximity_data.mode = buf_value ? ENABLE : DISABLE;
	FTS_DEBUG("[PROXIMITY] proximity mode = %d", fts_proximity_data.mode);
	return 0;
}

/*****************************************************************************
 *  Name: fts_proximity_recovery
 *  Brief: need call when reset
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
int fts_proximity_recovery(struct fts_ts_data *ts_data)
{
	int ret = 0;

	if (fts_proximity_data.mode)
		ret = fts_enter_proximity_mode(ENABLE);

	return ret;
}

#if FTS_ALSPS_SUPPORT


static int ps_open_report_data(int open)
{
	/* should queue work to report event if  is_report_input_direct=true */
	return 0;
}



static int ps_enable_nodata(int en)
{
	int err = 0;

	FTS_DEBUG("[PROXIMITY]SENSOR_ENABLE value = %d", en);
	/* Enable proximity */
	mutex_lock(&fts_data->input_dev->mutex);
	err = fts_enter_proximity_mode(en);
	mutex_unlock(&fts_data->input_dev->mutex);
	return err;
}

static int ps_set_delay(u64 ns)
{
	return 0;
}

static int ps_get_data(int *value, int *status)
{
	*value = (int)fts_proximity_data.detect;
	FTS_DEBUG("fts_proximity_data.detect = %d\n", *value);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	return 0;
}
int ps_local_init(void)
{
	int err = 0;
	struct ps_control_path ps_ctl = {0};
	struct ps_data_path ps_data = {0};

	ps_ctl.is_use_common_factory = false;
	ps_ctl.open_report_data = ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay = ps_set_delay;
	ps_ctl.is_report_input_direct = false;
	ps_ctl.is_support_batch = false;

	err = ps_register_control_path(&ps_ctl);
	if (err)
		FTS_ERROR("register fail = %d\n", err);

	ps_data.get_data = ps_get_data;
	ps_data.vender_div = 100;
	err = ps_register_data_path(&ps_data);
	if (err)
		FTS_ERROR("tregister fail = %d\n", err);


	return err;
}
int ps_local_uninit(void)
{
	return 0;
}

struct alsps_init_info ps_init_info = {
	.name = "fts_ts", .init = ps_local_init, .uninit = ps_local_uninit,
};

#else

/*****************************************************************************
 *  Name: fts_ps_operate
 *  Brief:
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
static int fts_ps_operate(void *self, uint32_t command, void *buff_in,
			  int size_in, void *buff_out, int size_out,
			  int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *sensor_data;

	FTS_DEBUG("[PROXIMITY]COMMAND = %d", command);
	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			FTS_ERROR("[PROXIMITY]Set delay parameter error!");
			err = -EINVAL;
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			FTS_ERROR("[PROXIMITY]Enable sensor parameter error!");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			FTS_DEBUG("[PROXIMITY]SENSOR_ENABLE value = %d", value);
			/* Enable proximity */
			err = fts_enter_proximity_mode(value);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) ||
		    (size_out < sizeof(struct hwm_sensor_data))) {
			FTS_ERROR(
				"[PROXIMITY]get sensor data parameter error!");
			err = -EINVAL;
		} else {
			sensor_data = (struct hwm_sensor_data *)buff_out;
			sensor_data->values[0] = (int)fts_proximity_data.detect;
			FTS_DEBUG("sensor_data->values[0] = %d",
				  sensor_data->values[0]);
			sensor_data->value_divide = 1;
			sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		}
		break;
	default:
		FTS_ERROR("[PROXIMITY]ps has no operate function:%d!", command);
		err = -EPERM;
		break;
	}

	return err;
}
#endif

/*****************************************************************************
 *  Name: fts_proximity_readdata
 *  Brief:
 *  Input:
 *  Output:
 *  Return: 0 - need return in suspend
 *****************************************************************************/
int fts_proximity_readdata(struct fts_ts_data *ts_data)
{
	int ret;
	int proximity_status = 1;
	u8 regvalue;
#if !FTS_ALSPS_SUPPORT
	struct hwm_sensor_data sensor_data;
#endif
	if (fts_proximity_data.mode == DISABLE)
		return -EPERM;

	fts_read_reg(FTS_REG_FACE_DEC_MODE_STATUS, &regvalue);

	if (regvalue == 0xC0) {
		/* close. need lcd off */
		proximity_status = PS_NEAR;
	} else if (regvalue == 0xE0) {
		/* far away */
		proximity_status = PS_FAR_AWAY;
	}

	FTS_INFO("fts_proximity_data.detect is %d", fts_proximity_data.detect);

	if (proximity_status != (int)fts_proximity_data.detect) {
		FTS_DEBUG("[PROXIMITY] p-sensor state:%s",
			  proximity_status ? "AWAY" : "NEAR");
		fts_proximity_data.detect =
			proximity_status ? PS_FAR_AWAY : PS_NEAR;
#if FTS_ALSPS_SUPPORT
		ret = ps_report_interrupt_data(fts_proximity_data.detect);
#else
		sensor_data.values[0] = proximity_status;
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
		ret = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data);
		if (ret) {
			FTS_ERROR(
				"[PROXIMITY] Call hwmsen_get_interrupt_data failed, ret=%d",
				ret);
			return ret;
		}
#endif
		return 0;
	}

	return -1;
}

/*****************************************************************************
 *  Name: fts_proximity_suspend
 *  Brief: Run when tp enter into suspend
 *  Input:
 *  Output:
 *  Return: 0 - need return in suspend
 *****************************************************************************/
int fts_proximity_suspend(void)
{
	if (fts_proximity_data.mode == ENABLE)
		return 0;
	else
		return -1;
}

/*****************************************************************************
 *  Name: fts_proximity_resume
 *  Brief: Run when tp resume
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
int fts_proximity_resume(void)
{
	if (fts_proximity_data.mode == ENABLE)
		return 0;
	else
		return -1;
}

int fts_proximity_init(void)
{
#if !FTS_ALSPS_SUPPORT
	int err = 0;
	struct hwmsen_object obj_ps;
#endif

	FTS_FUNC_ENTER();

	memset((u8 *)&fts_proximity_data, 0, sizeof(struct fts_proximity_st));
	fts_proximity_data.detect = PS_FAR_AWAY; /* defalut far awway */

#if FTS_ALSPS_SUPPORT
	alsps_driver_add(&ps_init_info);
#else
	obj_ps.polling = 0; /* interrupt mode */
	obj_ps.sensor_operate = fts_ps_operate;
	err = hwmsen_attach(ID_PROXIMITY, &obj_ps);
	if (err)
		FTS_ERROR("[PROXIMITY]fts proximity attach fail = %d!", err);
	else
		FTS_INFO("[PROXIMITY]fts proximity attach ok = %d\n", err);
#endif

	FTS_FUNC_EXIT();
	return 0;
}

int fts_proximity_exit(void)
{
	return 0;
}
#endif /* FTS_PSENSOR_EN */
