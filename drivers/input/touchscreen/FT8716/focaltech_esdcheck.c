/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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
* File Name: focaltech_esdcheck.c
*
*    Author: luoguojin
*
*   Created: 2016-08-03
*
*  Abstract: ESD check function
*
*   Version: v1.0
*
* Revision History:
*        v1.0:
*            First release. By luougojin 2016-08-03
*        v1.1: By luougojin 2017-02-15
*            1. Add LCD_ESD_PATCH to control idc_esdcheck_lcderror
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "focaltech_core.h"

#if FTS_ESDCHECK_EN
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define ESDCHECK_WAIT_TIME              4000
#define LCD_ESD_PATCH                   0

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
struct fts_esdcheck_st {
	u8      active:1;    /* 1- esd check active, need check esd 0- no esd check */
	u8      suspend:1;
	u8      proc_debug:1;    /* apk or adb is accessing I2C */
	u8      intr:1;    /* 1- Interrupt trigger */
	u8      unused:4;
	u8      flow_work_hold_cnt;         /* Flow Work Cnt(reg0x91) keep a same value for x times. >=5 times is ESD, need reset */
	u8      flow_work_cnt_last;         /* Save Flow Work Cnt(reg0x91) value */
	u32     hardware_reset_cnt;
	u32     i2c_nack_cnt;
	u32     i2c_dataerror_cnt;
};

/*****************************************************************************
* Static variables
*****************************************************************************/
static struct delayed_work fts_esdcheck_work;
static struct workqueue_struct *fts_esdcheck_workqueue;
static struct fts_esdcheck_st fts_esdcheck_data;

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

/*****************************************************************************
* functions body
*****************************************************************************/
#if LCD_ESD_PATCH
/*****************************************************************************
*  Name: lcd_esdcheck
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
int lcd_need_reset;
static int tp_need_recovery; /* LCD reset cause Tp reset */
int idc_esdcheck_lcderror(void)
{
	u8 val;
	int ret;

	FTS_DEBUG("[ESD]Check LCD ESD");
	if ((tp_need_recovery == 1) && (lcd_need_reset == 0)) {
		tp_need_recovery = 0;
		/* LCD reset, need recover TP state */
		fts_tp_state_recovery(fts_i2c_client);
	}

	ret = fts_i2c_read_reg(fts_i2c_client, FTS_REG_ESD_SATURATE, &val);
	if (ret < 0) {
		FTS_ERROR("[ESD]: Read ESD_SATURATE(0xED) failed ret=%d!", ret);
		return -EIO;
	}

	if (val == 0xAA) {
		/*
		* 1. Set flag lcd_need_reset = 1;
		* 2. LCD driver need reset(recovery) LCD and set lcd_need_reset to 0
		* 3. recover TP state
		*/
		FTS_INFO("LCD ESD, Execute LCD reset!");
		lcd_need_reset = 1;
		tp_need_recovery = 1;
	}

	return 0;
}
#endif

/*****************************************************************************
*  Name: fts_esdcheck_tp_reset
*  Brief: esd check algorithm
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_esdcheck_tp_reset(void)
{
	FTS_FUNC_ENTER();

	fts_esdcheck_data.flow_work_hold_cnt = 0;
	fts_esdcheck_data.hardware_reset_cnt++;

	fts_reset_proc(200);
	fts_tp_state_recovery(fts_i2c_client);

	FTS_FUNC_EXIT();
	return 0;
}

/*****************************************************************************
*  Name: get_chip_id
*  Brief: Read Chip Id 3 times
*  Input:
*  Output:
*  Return:  1 - Read Chip Id 3 times failed
*           0 - Read Chip Id pass
*****************************************************************************/
static bool get_chip_id(void)
{
	int     err = 0;
	int     i = 0;
	u8      reg_value = 0;
	u8      reg_addr = 0;

	for (i = 0; i < 3; i++) {
		reg_addr = FTS_REG_CHIP_ID;
		err = fts_i2c_read(fts_i2c_client, &reg_addr, 1, &reg_value, 1);

		if (err < 0) {
			FTS_ERROR("[ESD]: Read Reg 0xA3 failed ret = %d!!", err);
			fts_esdcheck_data.i2c_nack_cnt++;
		} else {
			if ((reg_value == chip_types.chip_idh) || (reg_value == 0xEF)) {/* Upgrade sometimes can't detect */
				break;
			} else {
				fts_esdcheck_data.i2c_dataerror_cnt++;
			}
		}
	}

	/* if can't get correct data in 3 times, then need hardware reset */
	if (i >= 3) {
		FTS_ERROR("[ESD]: Read Chip id 3 times failed, need execute TP reset!!");
		return 1;
	}

	return 0;
}

/*****************************************************************************
*  Name: get_flow_cnt
*  Brief: Read flow cnt(0x91)
*  Input:
*  Output:
*  Return:  1 - Reg 0x91(flow cnt) abnormal: hold a value for 5 times
*           0 - Reg 0x91(flow cnt) normal
*****************************************************************************/
static bool get_flow_cnt(void)
{
	int     err = 0;
	u8      reg_value = 0;
	u8      reg_addr = 0;

	reg_addr = FTS_REG_FLOW_WORK_CNT;
	err = fts_i2c_read(fts_i2c_client, &reg_addr, 1, &reg_value, 1);
	if (err < 0) {
		FTS_ERROR("[ESD]: Read Reg 0x91 failed ret = %d!!", err);
		fts_esdcheck_data.i2c_nack_cnt++;
	} else {
		if (reg_value == fts_esdcheck_data.flow_work_cnt_last) {
			fts_esdcheck_data.flow_work_hold_cnt++;
		} else {
			fts_esdcheck_data.flow_work_hold_cnt = 0;
		}

		fts_esdcheck_data.flow_work_cnt_last = reg_value;
	}

	/* if read flow work cnt 5 times and the value are all the same, then need hardware_reset */
	if (fts_esdcheck_data.flow_work_hold_cnt >= 5) {
		FTS_DEBUG("[ESD]: Flow Work Cnt(reg0x91) keep a value for 5 times, need execute TP reset!!");
		return 1;
	}

	return 0;
}

extern int panel_dead2tp;
/*****************************************************************************
*  Name: esdcheck_algorithm
*  Brief: esd check algorithm
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int esdcheck_algorithm(void)
{
	int     err = 0;
	u8      reg_value = 0;
	u8      reg_addr = 0;
	bool    hardware_reset = 0;

	/* 1. esdcheck is interrupt, then return */
	if (fts_esdcheck_data.intr == 1) {
		FTS_INFO("[ESD]: In interrupt state, not check esd, return immediately!!");
		return 0;
	}

	/* 2. check power state, if suspend, no need check esd */
	if (fts_esdcheck_data.suspend == 1) {
		FTS_INFO("[ESD]: In suspend, not check esd, return immediately!!");
		/* because in suspend state, adb can be used, when upgrade FW, will active ESD check(active = 1)
		*  But in suspend, then will don't queue_delayed_work, when resume, don't check ESD again
		*/
		fts_esdcheck_data.active = 0;
		return 0;
	}

	/* 3. check fts_esdcheck_data.proc_debug state, if 1-proc busy, no need check esd*/
	if (fts_esdcheck_data.proc_debug == 1) {
		FTS_INFO("[ESD]: In apk or adb command mode, not check esd, return immediately!!");
		return 0;
	}

	/* 4. In factory mode, can't check esd */
	reg_addr = FTS_REG_WORKMODE;
	err = fts_i2c_read(fts_i2c_client, &reg_addr, 1, &reg_value, 1);
	if (err < 0) {
		fts_esdcheck_data.i2c_nack_cnt++;
	} else if ((reg_value & 0x70) ==  FTS_REG_WORKMODE_FACTORY_VALUE) {
		FTS_INFO("[ESD]: In factory mode, not check esd, return immediately!!");
		return 0;
	}

	/* 5. IDC esd check lcd  default:close */
#if LCD_ESD_PATCH
	idc_esdcheck_lcderror();
#endif

	/* 6. Get Chip ID */
	hardware_reset = get_chip_id();

	/* 7. get Flow work cnt: 0x91 If no change for 5 times, then ESD and reset */
	if (!hardware_reset) {

		hardware_reset = get_flow_cnt();
	}

reg_addr = 0xED;
fts_i2c_read(fts_i2c_client, &reg_addr, 1, &reg_value, 1);
if (reg_value == 0xAA) {
	panel_dead2tp = 1;
	printk("FTS:change panel_dead2tp:%d\n", panel_dead2tp);
}
	/* 8. If need hardware reset, then handle it here */
	if (hardware_reset == 1) {
		fts_esdcheck_tp_reset();
	}


	return 0;
}

/*****************************************************************************
*  Name: fts_esdcheck_func
*  Brief: fts_esdcheck_func
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void esdcheck_func(struct work_struct *work)
{
	FTS_FUNC_ENTER();

	esdcheck_algorithm();

	if (fts_esdcheck_data.suspend == 0) {
		queue_delayed_work(fts_esdcheck_workqueue, &fts_esdcheck_work, msecs_to_jiffies(ESDCHECK_WAIT_TIME));
	}

	FTS_FUNC_EXIT();
}

/*****************************************************************************
*  Name: fts_esdcheck_set_intr
*  Brief: interrupt flag (main used in interrupt tp report)
*  Input:
*  Output:
*  Return:
*****************************************************************************/
int fts_esdcheck_set_intr(bool intr)
{
	/* interrupt don't add debug message */
	fts_esdcheck_data.intr = intr;
	return 0;
}

/*****************************************************************************
*  Name: fts_esdcheck_get_status(void)
*  Brief: get current status
*  Input:
*  Output:
*  Return:
*****************************************************************************/
int fts_esdcheck_get_status(void)
{
	/* interrupt don't add debug message */
	return fts_esdcheck_data.active;
}

/*****************************************************************************
*  Name: fts_esdcheck_proc_busy
*  Brief: When APK or ADB command access TP via driver, then need set proc_debug,
*         then will not check ESD.
*  Input:
*  Output:
*  Return:
*****************************************************************************/
int fts_esdcheck_proc_busy(bool proc_debug)
{
	fts_esdcheck_data.proc_debug = proc_debug;
	return 0;
}

/*****************************************************************************
*  Name: fts_esdcheck_switch
*  Brief: FTS esd check function switch.
*  Input:   enable:  1 - Enable esd check
*                    0 - Disable esd check
*  Output:
*  Return:
*****************************************************************************/
int fts_esdcheck_switch(bool enable)
{
	FTS_FUNC_ENTER();
	if (enable == 1) {
		if (fts_esdcheck_data.active == 0) {
			FTS_INFO("[ESD]: ESD check start!!");
			fts_esdcheck_data.active = 1;
			queue_delayed_work(fts_esdcheck_workqueue, &fts_esdcheck_work, msecs_to_jiffies(ESDCHECK_WAIT_TIME));
		}
	} else {
		if (fts_esdcheck_data.active == 1) {
			FTS_INFO("[ESD]: ESD check stop!!");
			fts_esdcheck_data.active = 0;
			cancel_delayed_work_sync(&fts_esdcheck_work);
		}
	}

	FTS_FUNC_EXIT();
	return 0;
}

/*****************************************************************************
*  Name: fts_esdcheck_suspend
*  Brief: Run when tp enter into suspend
*  Input:
*  Output:
*  Return:
*****************************************************************************/
int fts_esdcheck_suspend(void)
{
	FTS_FUNC_ENTER();
	fts_esdcheck_switch(DISABLE);
	fts_esdcheck_data.suspend = 1;
	FTS_FUNC_EXIT();
	return 0;
}

/*****************************************************************************
*  Name: fts_esdcheck_resume
*  Brief: Run when tp resume
*  Input:
*  Output:
*  Return:
*****************************************************************************/
int fts_esdcheck_resume(void)
{
	FTS_FUNC_ENTER();
	fts_esdcheck_switch(ENABLE);
	fts_esdcheck_data.suspend = 0;
	FTS_FUNC_EXIT();
	return 0;
}


/*****************************************************************************
*  Name: fts_esdcheck_init
*  Brief: Init and create a queue work to check esd
*  Input:
*  Output:
*  Return: < 0: Fail to create esd check queue
*****************************************************************************/
int fts_esdcheck_init(void)
{
	FTS_FUNC_ENTER();

	INIT_DELAYED_WORK(&fts_esdcheck_work, esdcheck_func);
	fts_esdcheck_workqueue = create_workqueue("fts_esdcheck_wq");
	if (fts_esdcheck_workqueue == NULL) {
		FTS_INFO("[ESD]: Failed to create esd work queue!!");
	}

	memset((u8 *)&fts_esdcheck_data, 0, sizeof(struct fts_esdcheck_st));

	fts_esdcheck_switch(ENABLE);
	FTS_FUNC_EXIT();
	return 0;
}

/*****************************************************************************
*  Name: fts_esdcheck_exit
*  Brief: When FTS TP driver is removed, then call this function to destory work queue
*  Input:
*  Output:
*  Return:
*****************************************************************************/
int fts_esdcheck_exit(void)
{
	FTS_FUNC_ENTER();

	destroy_workqueue(fts_esdcheck_workqueue);

	FTS_FUNC_EXIT();
	return 0;
}
#endif /* FTS_ESDCHECK_EN */

