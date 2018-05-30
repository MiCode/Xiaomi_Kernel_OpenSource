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

#include "focaltech_core.h"

#if FTS_ESDCHECK_EN
#define ESDCHECK_WAIT_TIME              1000
#define LCD_ESD_PATCH                   0

struct fts_esdcheck_st
{
	u8      active              : 1;
	u8      suspend             : 1;
	u8      proc_debug          : 1;
	u8      intr                : 1;
	u8      unused              : 4;
	u8      flow_work_hold_cnt;
	u8      flow_work_cnt_last;
	u32     hardware_reset_cnt;
	u32     i2c_nack_cnt;
	u32     i2c_dataerror_cnt;
};

static struct delayed_work fts_esdcheck_work;
static struct workqueue_struct *fts_esdcheck_workqueue = NULL;
static struct fts_esdcheck_st fts_esdcheck_data;

#if LCD_ESD_PATCH
int lcd_need_reset;
static int tp_need_recovery;
int idc_esdcheck_lcderror(void)
{
	u8 val;
	int ret;

	FTS_DEBUG("[ESD]Check LCD ESD");
	if ( (tp_need_recovery == 1) && (lcd_need_reset == 0) ) {
		 tp_need_recovery = 0;
		 fts_tp_state_recovery(fts_i2c_client);
	}

	ret = fts_i2c_read_reg(fts_i2c_client, FTS_REG_ESD_SATURATE, &val);
	if ( ret < 0) {
		 FTS_ERROR("[ESD]: Read ESD_SATURATE(0xED) failed ret=%d!", ret);
		 return -EIO;
	}

	if (val == 0xAA) {
		 FTS_INFO("LCD ESD, Execute LCD reset!");
		 lcd_need_reset = 1;
		 tp_need_recovery = 1;
	}

	return 0;
}
#endif

static int fts_esdcheck_tp_reset( void )
{
	FTS_FUNC_ENTER();

	fts_esdcheck_data.flow_work_hold_cnt = 0;
	fts_esdcheck_data.hardware_reset_cnt++;

	fts_reset_proc(200);
	fts_tp_state_recovery(fts_i2c_client);

	FTS_FUNC_EXIT();
	return 0;
}

static bool get_chip_id( void )
{
	int     err = 0;
	int     i = 0;
	u8      reg_value = 0;
	u8      reg_addr = 0;

	for (i = 0; i < 3; i++) {
		 reg_addr = FTS_REG_CHIP_ID;
		 err = fts_i2c_read(fts_i2c_client, &reg_addr, 1, &reg_value, 1);

		 if ( err < 0 ) {
			FTS_ERROR("[ESD]: Read Reg 0xA3 failed ret = %d!!", err);
			fts_esdcheck_data.i2c_nack_cnt++;
		 } else {
			if ( (reg_value == chip_types.chip_idh) || (reg_value == 0xEF) ) /* Upgrade sometimes can't detect */ {
				break;
			} else {
				fts_esdcheck_data.i2c_dataerror_cnt++;
			}
		 }
	}


	if (i >= 3) {
		 FTS_ERROR("[ESD]: Read Chip id 3 times failed, need execute TP reset!!");
		 return 1;
	}

	return 0;
}

static bool get_flow_cnt( void )
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
		 if ( reg_value == fts_esdcheck_data.flow_work_cnt_last ) {
			fts_esdcheck_data.flow_work_hold_cnt++;
		 } else {
			fts_esdcheck_data.flow_work_hold_cnt = 0;
		 }

		 fts_esdcheck_data.flow_work_cnt_last = reg_value;
	}


	if (fts_esdcheck_data.flow_work_hold_cnt >= 5) {
		 FTS_DEBUG("[ESD]: Flow Work Cnt(reg0x91) keep a value for 5 times, need execute TP reset!!");
		 return 1;
	}

	return 0;
}

static int esdcheck_algorithm(void)
{
	int     err = 0;
	u8      reg_value = 0;
	u8      reg_addr = 0;
	bool    hardware_reset = 0;


	if (fts_esdcheck_data.intr == 1) {
		 FTS_INFO("[ESD]: In interrupt state, not check esd, return immediately!!");
		 return 0;
	}


	if (fts_esdcheck_data.suspend == 1) {
		 FTS_INFO("[ESD]: In suspend, not check esd, return immediately!!");
		 fts_esdcheck_data.active = 0;
		 return 0;
	}


	if (fts_esdcheck_data.proc_debug == 1) {
		 FTS_INFO("[ESD]: In apk or adb command mode, not check esd, return immediately!!");
		 return 0;
	}


	reg_addr = FTS_REG_WORKMODE;
	err = fts_i2c_read(fts_i2c_client, &reg_addr, 1, &reg_value, 1);
	if ( err < 0 ) {
		 fts_esdcheck_data.i2c_nack_cnt++;
	} else if ( (reg_value & 0x70) ==  FTS_REG_WORKMODE_FACTORY_VALUE) {
		 FTS_INFO("[ESD]: In factory mode, not check esd, return immediately!!");
		 return 0;
	}


#if LCD_ESD_PATCH
	idc_esdcheck_lcderror();
#endif


	hardware_reset = get_chip_id();


	if (!hardware_reset) {
		 hardware_reset = get_flow_cnt();
	}


	if ( hardware_reset == 1) {
		 fts_esdcheck_tp_reset();
	}


	return 0;
}

static void esdcheck_func(struct work_struct *work)
{


	esdcheck_algorithm();

	if ( fts_esdcheck_data.suspend == 0 ) {
		 queue_delayed_work(fts_esdcheck_workqueue, &fts_esdcheck_work, msecs_to_jiffies(ESDCHECK_WAIT_TIME));
	}


}
int fts_esdcheck_set_intr(bool intr)
{

	fts_esdcheck_data.intr = intr;
	return 0;
}

int fts_esdcheck_get_status(void)
{

	return fts_esdcheck_data.active;
}
int fts_esdcheck_proc_busy(bool proc_debug)
{
	fts_esdcheck_data.proc_debug = proc_debug;
	return 0;
}

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

int fts_esdcheck_suspend(void)
{
	FTS_FUNC_ENTER();
	fts_esdcheck_switch(DISABLE);
	fts_esdcheck_data.suspend = 1;
	FTS_FUNC_EXIT();
	return 0;
}

int fts_esdcheck_resume( void )
{
	FTS_FUNC_ENTER();
	fts_esdcheck_switch(ENABLE);
	fts_esdcheck_data.suspend = 0;
	FTS_FUNC_EXIT();
	return 0;
}

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

int fts_esdcheck_exit(void)
{
	FTS_FUNC_ENTER();

	destroy_workqueue(fts_esdcheck_workqueue);

	FTS_FUNC_EXIT();
	return 0;
}
#endif /* FTS_ESDCHECK_EN */

