/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/*! \file
*    \brief  Declaration of library functions
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/

#define pr_fmt(fmt) KBUILD_MODNAME "@(%s:%d) " fmt, __func__, __LINE__

#include "consys_hw.h"
#include "conninfra_core.h"
#include "msg_thread.h"
#include "consys_reg_mng.h"
#include "conninfra_conf.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define CONNINFRA_EVENT_TIMEOUT 3000
#define CONNINFRA_RESET_TIMEOUT 500
#define CONNINFRA_PRE_CAL_TIMEOUT 500

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/delay.h>

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

static int opfunc_power_on(struct msg_op_data *op);
static int opfunc_power_off(struct msg_op_data *op);
static int opfunc_chip_rst(struct msg_op_data *op);
static int opfunc_pre_cal(struct msg_op_data *op);
static int opfunc_therm_ctrl(struct msg_op_data *op);
static int opfunc_rfspi_read(struct msg_op_data *op);
static int opfunc_rfspi_write(struct msg_op_data *op);
static int opfunc_adie_top_ck_en_on(struct msg_op_data *op);
static int opfunc_adie_top_ck_en_off(struct msg_op_data *op);
static int opfunc_spi_clock_switch(struct msg_op_data *op);
static int opfunc_clock_fail_dump(struct msg_op_data *op);
static int opfunc_pre_cal_prepare(struct msg_op_data *op);
static int opfunc_pre_cal_check(struct msg_op_data *op);

static int opfunc_force_conninfra_wakeup(struct msg_op_data *op);
static int opfunc_force_conninfra_sleep(struct msg_op_data *op);

static int opfunc_dump_power_state(struct msg_op_data *op);

static int opfunc_subdrv_pre_reset(struct msg_op_data *op);
static int opfunc_subdrv_post_reset(struct msg_op_data *op);
static int opfunc_subdrv_cal_pwr_on(struct msg_op_data *op);
static int opfunc_subdrv_cal_do_cal(struct msg_op_data *op);
static int opfunc_subdrv_therm_ctrl(struct msg_op_data *op);

static void _conninfra_core_update_rst_status(enum chip_rst_status status);

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

struct conninfra_ctx g_conninfra_ctx;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static const msg_opid_func conninfra_core_opfunc[] = {
	[CONNINFRA_OPID_PWR_ON] = opfunc_power_on,
	[CONNINFRA_OPID_PWR_OFF] = opfunc_power_off,
	[CONNINFRA_OPID_THERM_CTRL] = opfunc_therm_ctrl,
	[CONNINFRA_OPID_RFSPI_READ] = opfunc_rfspi_read,
	[CONNINFRA_OPID_RFSPI_WRITE] = opfunc_rfspi_write,
	[CONNINFRA_OPID_ADIE_TOP_CK_EN_ON] = opfunc_adie_top_ck_en_on,
	[CONNINFRA_OPID_ADIE_TOP_CK_EN_OFF] = opfunc_adie_top_ck_en_off,
	[CONNINFRA_OPID_SPI_CLOCK_SWITCH] = opfunc_spi_clock_switch,
	[CONNINFRA_OPID_CLOCK_FAIL_DUMP] = opfunc_clock_fail_dump,
	[CONNINFRA_OPID_PRE_CAL_PREPARE] = opfunc_pre_cal_prepare,
	[CONNINFRA_OPID_PRE_CAL_CHECK] = opfunc_pre_cal_check,

	[CONNINFRA_OPID_FORCE_CONNINFRA_WAKUP] = opfunc_force_conninfra_wakeup,
	[CONNINFRA_OPID_FORCE_CONNINFRA_SLEEP] = opfunc_force_conninfra_sleep,

	[CONNINFRA_OPID_DUMP_POWER_STATE] = opfunc_dump_power_state,
};

static const msg_opid_func conninfra_core_cb_opfunc[] = {
	[CONNINFRA_CB_OPID_CHIP_RST] = opfunc_chip_rst,
	[CONNINFRA_CB_OPID_PRE_CAL] = opfunc_pre_cal,
};


/* subsys ops */
static char *drv_thread_name[] = {
	[CONNDRV_TYPE_BT] = "sub_bt_thrd",
	[CONNDRV_TYPE_FM] = "sub_fm_thrd",
	[CONNDRV_TYPE_GPS] = "sub_gps_thrd",
	[CONNDRV_TYPE_WIFI] = "sub_wifi_thrd",
	[CONNDRV_TYPE_CONNINFRA] = "sub_conninfra_thrd",
};

static char *drv_name[] = {
	[CONNDRV_TYPE_BT] = "BT",
	[CONNDRV_TYPE_FM] = "FM",
	[CONNDRV_TYPE_GPS] = "GPS",
	[CONNDRV_TYPE_WIFI] = "WIFI",
	[CONNDRV_TYPE_CONNINFRA] = "CONNINFRA",
};

typedef enum {
	INFRA_SUBDRV_OPID_PRE_RESET	= 0,
	INFRA_SUBDRV_OPID_POST_RESET	= 1,
	INFRA_SUBDRV_OPID_CAL_PWR_ON	= 2,
	INFRA_SUBDRV_OPID_CAL_DO_CAL	= 3,
	INFRA_SUBDRV_OPID_THERM_CTRL	= 4,

	INFRA_SUBDRV_OPID_MAX
} infra_subdrv_op;


static const msg_opid_func infra_subdrv_opfunc[] = {
	[INFRA_SUBDRV_OPID_PRE_RESET] = opfunc_subdrv_pre_reset,
	[INFRA_SUBDRV_OPID_POST_RESET] = opfunc_subdrv_post_reset,
	[INFRA_SUBDRV_OPID_CAL_PWR_ON] = opfunc_subdrv_cal_pwr_on,
	[INFRA_SUBDRV_OPID_CAL_DO_CAL] = opfunc_subdrv_cal_do_cal,
	[INFRA_SUBDRV_OPID_THERM_CTRL] = opfunc_subdrv_therm_ctrl,
};

enum pre_cal_type {
	PRE_CAL_ALL_ENABLED = 0,
	PRE_CAL_ALL_DISABLED = 1,
	PRE_CAL_PWR_ON_DISABLED = 2,
	PRE_CAL_SCREEN_ON_DISABLED = 3
};

static unsigned int g_pre_cal_mode = 0;

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

static void reset_chip_rst_trg_data(void)
{
	g_conninfra_ctx.trg_drv = CONNDRV_TYPE_MAX;
	memset(g_conninfra_ctx.trg_reason, '\0', CHIP_RST_REASON_MAX_LEN);
}

static unsigned long timeval_to_ms(struct timeval *begin, struct timeval *end)
{
	unsigned long time_diff;

	time_diff = (end->tv_sec - begin->tv_sec) * 1000;
	time_diff += (end->tv_usec - begin->tv_usec) / 1000;

	return time_diff;
}

static unsigned int opfunc_get_current_status(void)
{
	unsigned int ret = 0;
	unsigned int i;

	for (i = 0; i < CONNDRV_TYPE_MAX; i++) {
		ret |= (g_conninfra_ctx.drv_inst[i].drv_status << i);
	}

	return ret;
}

static void opfunc_vcn_control_internal(unsigned int drv_type, bool on)
{
	/* VCNx enable */
	switch (drv_type) {
	case CONNDRV_TYPE_BT:
		consys_hw_bt_power_ctl(on);
		break;
	case CONNDRV_TYPE_FM:
		consys_hw_fm_power_ctl(on);
		break;
	case CONNDRV_TYPE_GPS:
		consys_hw_gps_power_ctl(on);
		break;
	case CONNDRV_TYPE_WIFI:
		consys_hw_wifi_power_ctl(on);
		break;
	default:
		pr_err("Wrong parameter: drv_type(%d)\n", drv_type);
		break;
	}
}

static int opfunc_power_on_internal(unsigned int drv_type)
{
	int ret;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	/* Check abnormal type */
	if (drv_type >= CONNDRV_TYPE_MAX) {
		pr_err("abnormal Fun(%d)\n", drv_type);
		return -EINVAL;
	}

	/* Check abnormal state */
	if ((g_conninfra_ctx.drv_inst[drv_type].drv_status < DRV_STS_POWER_OFF)
	    || (g_conninfra_ctx.drv_inst[drv_type].drv_status >= DRV_STS_MAX)) {
		pr_err("func(%d) status[0x%x] abnormal\n", drv_type,
				g_conninfra_ctx.drv_inst[drv_type].drv_status);
		return -EINVAL;
	}

	ret = osal_lock_sleepable_lock(&infra_ctx->core_lock);
	if (ret) {
		pr_err("core_lock fail!!");
		return ret;
	}

	/* check if func already on */
	if (g_conninfra_ctx.drv_inst[drv_type].drv_status == DRV_STS_POWER_ON) {
		pr_warn("func(%d) already on\n", drv_type);
		osal_unlock_sleepable_lock(&infra_ctx->core_lock);
		return 0;
	}

	ret = consys_hw_pwr_on(opfunc_get_current_status(), drv_type);
	if (ret) {
		pr_err("Conninfra power on fail. drv(%d) ret=(%d)\n",
			drv_type, ret);
		osal_unlock_sleepable_lock(&infra_ctx->core_lock);
		return -3;
	}
	/* POWER ON SEQUENCE */
	g_conninfra_ctx.infra_drv_status = DRV_STS_POWER_ON;

	g_conninfra_ctx.drv_inst[drv_type].drv_status = DRV_STS_POWER_ON;

	/* VCNx enable */
	opfunc_vcn_control_internal(drv_type, true);

	pr_info("[Conninfra Pwr On] BT=[%d] FM=[%d] GPS=[%d] WF=[%d]",
			infra_ctx->drv_inst[CONNDRV_TYPE_BT].drv_status,
			infra_ctx->drv_inst[CONNDRV_TYPE_FM].drv_status,
			infra_ctx->drv_inst[CONNDRV_TYPE_GPS].drv_status,
			infra_ctx->drv_inst[CONNDRV_TYPE_WIFI].drv_status);
	osal_unlock_sleepable_lock(&infra_ctx->core_lock);

	return 0;
}

static int opfunc_power_on(struct msg_op_data *op)
{
	unsigned int drv_type = op->op_data[0];

	return opfunc_power_on_internal(drv_type);
}

static int opfunc_power_off_internal(unsigned int drv_type)
{
	int i, ret;
	bool try_power_off = true;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;
	unsigned int curr_status = opfunc_get_current_status();

	/* Check abnormal type */
	if (drv_type >= CONNDRV_TYPE_MAX) {
		pr_err("abnormal Fun(%d)\n", drv_type);
		return -EINVAL;
	}

	ret = osal_lock_sleepable_lock(&infra_ctx->core_lock);
	if (ret) {
		pr_err("core_lock fail!!");
		return ret;
	}

	/* Check abnormal state */
	if ((g_conninfra_ctx.drv_inst[drv_type].drv_status < DRV_STS_POWER_OFF)
	    || (g_conninfra_ctx.drv_inst[drv_type].drv_status >= DRV_STS_MAX)) {
		pr_err("func(%d) status[0x%x] abnormal\n", drv_type,
			g_conninfra_ctx.drv_inst[drv_type].drv_status);
		osal_unlock_sleepable_lock(&infra_ctx->core_lock);
		return -2;
	}

	/* Special case for force power off */
	if (drv_type == CONNDRV_TYPE_CONNINFRA) {
		if (g_conninfra_ctx.infra_drv_status == DRV_STS_POWER_OFF) {
			pr_warn("Connsys already off, do nothing for force off\n");
			return 0;
		}
		/* Turn off subsys VCN and update record */
		for (i = 0; i < CONNDRV_TYPE_MAX; i++) {
			if (g_conninfra_ctx.drv_inst[i].drv_status == DRV_STS_POWER_ON) {
				opfunc_vcn_control_internal(i, false);
				g_conninfra_ctx.drv_inst[i].drv_status = DRV_STS_POWER_OFF;
			}
		}
		/* POWER OFF SEQUENCE */
		ret = consys_hw_pwr_off(0, drv_type);
		/* For force power off operation, ignore err code */
		if (ret)
			pr_err("Force power off fail. ret=%d\n", ret);
		try_power_off = true;
	} else {
		/* check if func already off */
		if (g_conninfra_ctx.drv_inst[drv_type].drv_status
					== DRV_STS_POWER_OFF) {
			pr_warn("func(%d) already off\n", drv_type);
			osal_unlock_sleepable_lock(&infra_ctx->core_lock);
			return 0;
		}
		/* VCNx disable */
		opfunc_vcn_control_internal(drv_type, false);
		g_conninfra_ctx.drv_inst[drv_type].drv_status = DRV_STS_POWER_OFF;
		/* is there subsys on ? */
		for (i = 0; i < CONNDRV_TYPE_MAX; i++)
			if (g_conninfra_ctx.drv_inst[i].drv_status == DRV_STS_POWER_ON)
				try_power_off = false;

		/* POWER OFF SEQUENCE */
		ret = consys_hw_pwr_off(curr_status, drv_type);
		if (ret) {
			pr_err("Conninfra power on fail. drv(%d) ret=(%d)\n",
				drv_type, ret);
			osal_unlock_sleepable_lock(&infra_ctx->core_lock);
			return -3;
		}
	}

	if (try_power_off)
		g_conninfra_ctx.infra_drv_status = DRV_STS_POWER_OFF;

	pr_info("[Conninfra Pwr Off] Conninfra=[%d] BT=[%d] FM=[%d] GPS=[%d] WF=[%d]",
			infra_ctx->infra_drv_status,
			infra_ctx->drv_inst[CONNDRV_TYPE_BT].drv_status,
			infra_ctx->drv_inst[CONNDRV_TYPE_FM].drv_status,
			infra_ctx->drv_inst[CONNDRV_TYPE_GPS].drv_status,
			infra_ctx->drv_inst[CONNDRV_TYPE_WIFI].drv_status);

	osal_unlock_sleepable_lock(&infra_ctx->core_lock);
	return 0;
}

static int opfunc_power_off(struct msg_op_data *op)
{
	unsigned int drv_type = op->op_data[0];

	return opfunc_power_off_internal(drv_type);
}

static int opfunc_chip_rst(struct msg_op_data *op)
{
	int i, ret, cur_rst_state;
	struct subsys_drv_inst *drv_inst;
	unsigned int drv_pwr_state[CONNDRV_TYPE_MAX];
	const unsigned int subdrv_all_done = (0x1 << CONNDRV_TYPE_MAX) - 1;
	struct timeval pre_begin, pre_end, reset_end, done_end;

	if (g_conninfra_ctx.infra_drv_status == DRV_STS_POWER_OFF) {
		pr_info("No subsys on, just return");
		_conninfra_core_update_rst_status(CHIP_RST_NONE);
		return 0;
	}

	do_gettimeofday(&pre_begin);

	atomic_set(&g_conninfra_ctx.rst_state, 0);
	sema_init(&g_conninfra_ctx.rst_sema, 1);

	_conninfra_core_update_rst_status(CHIP_RST_PRE_CB);

	/* pre */
	for (i = 0; i < CONNDRV_TYPE_MAX; i++) {
		drv_inst = &g_conninfra_ctx.drv_inst[i];
		drv_pwr_state[i] = drv_inst->drv_status;
		pr_info("subsys %d is %d", i, drv_inst->drv_status);
		ret = msg_thread_send_1(&drv_inst->msg_ctx,
				INFRA_SUBDRV_OPID_PRE_RESET, i);
	}

	pr_info("[chip_rst] pre vvvvvvvvvvvvv");
	while (atomic_read(&g_conninfra_ctx.rst_state) != subdrv_all_done) {
		ret = down_timeout(&g_conninfra_ctx.rst_sema, msecs_to_jiffies(CONNINFRA_RESET_TIMEOUT));
		pr_info("sema ret=[%d]", ret);
		if (ret == 0)
			continue;
		cur_rst_state = atomic_read(&g_conninfra_ctx.rst_state);
		pr_info("cur_rst state =[%d]", cur_rst_state);
		for (i = 0; i < CONNDRV_TYPE_MAX; i++) {
			if ((cur_rst_state & (0x1 << i)) == 0) {
				pr_info("[chip_rst] [%s] pre-callback is not back", drv_thread_name[i]);
				drv_inst = &g_conninfra_ctx.drv_inst[i];
				osal_thread_show_stack(&drv_inst->msg_ctx.thread);
			}
		}
	}

	_conninfra_core_update_rst_status(CHIP_RST_RESET);

	do_gettimeofday(&pre_end);

	pr_info("[chip_rst] reset ++++++++++++");
	/*******************************************************/
	/* reset */
	/* call consys_hw */
	/*******************************************************/
	/* Special power-off function, turn off connsys directly */
	ret = opfunc_power_off_internal(CONNDRV_TYPE_CONNINFRA);
	pr_info("Force conninfra power off, ret=%d\n", ret);
	pr_info("conninfra status should be power off. Status=%d", g_conninfra_ctx.infra_drv_status);

	/* Turn on subsys */
	for (i = 0; i < CONNDRV_TYPE_MAX; i++) {
		if (drv_pwr_state[i]) {
			ret = opfunc_power_on_internal(i);
			pr_info("Call subsys(%d) power on ret=%d", i, ret);
		}
	}
	pr_info("conninfra status should be power on. Status=%d", g_conninfra_ctx.infra_drv_status);

	pr_info("[chip_rst] reset --------------");

	_conninfra_core_update_rst_status(CHIP_RST_POST_CB);

	do_gettimeofday(&reset_end);

	/* post */
	atomic_set(&g_conninfra_ctx.rst_state, 0);
	sema_init(&g_conninfra_ctx.rst_sema, 1);
	for (i = 0; i < CONNDRV_TYPE_MAX; i++) {
		drv_inst = &g_conninfra_ctx.drv_inst[i];
		ret = msg_thread_send_1(&drv_inst->msg_ctx,
				INFRA_SUBDRV_OPID_POST_RESET, i);
	}

	while (atomic_read(&g_conninfra_ctx.rst_state) != subdrv_all_done) {
		ret = down_timeout(&g_conninfra_ctx.rst_sema, msecs_to_jiffies(CONNINFRA_RESET_TIMEOUT));
		if (ret == 0)
			continue;
		cur_rst_state = atomic_read(&g_conninfra_ctx.rst_state);
		for (i = 0; i < CONNDRV_TYPE_MAX; i++) {
			if ((cur_rst_state & (0x1 << i)) == 0) {
				pr_info("[chip_rst] [%s] post-callback is not back", drv_thread_name[i]);
				drv_inst = &g_conninfra_ctx.drv_inst[i];
				osal_thread_show_stack(&drv_inst->msg_ctx.thread);
			}
		}
	}
	pr_info("[chip_rst] post ^^^^^^^^^^^^^^");

	reset_chip_rst_trg_data();
	//_conninfra_core_update_rst_status(CHIP_RST_DONE);
	_conninfra_core_update_rst_status(CHIP_RST_NONE);
	do_gettimeofday(&done_end);

	pr_info("[chip_rst] summary pre=[%lu] reset=[%lu] post=[%lu]",
				timeval_to_ms(&pre_begin, &pre_end),
				timeval_to_ms(&pre_end, &reset_end),
				timeval_to_ms(&reset_end, &done_end));

	return 0;
}

static int opfunc_pre_cal(struct msg_op_data *op)
{
#define CAL_DRV_COUNT 2
	int cal_drvs[CAL_DRV_COUNT] = {CONNDRV_TYPE_BT, CONNDRV_TYPE_WIFI};
	int i, ret, cur_state;
	int bt_cal_ret, wf_cal_ret;
	struct subsys_drv_inst *drv_inst;
	int pre_cal_done_state = (0x1 << CONNDRV_TYPE_BT) | (0x1 << CONNDRV_TYPE_WIFI);
	struct timeval begin, bt_cal_begin, wf_cal_begin, end;

	/* Check BT/WIFI status again */
	ret = osal_lock_sleepable_lock(&g_conninfra_ctx.core_lock);
	if (ret) {
		pr_err("[%s] core_lock fail!!", __func__);
		return ret;
	}
	/* check if func already on */
	for (i = 0; i < CAL_DRV_COUNT; i++) {
		if (g_conninfra_ctx.drv_inst[cal_drvs[i]].drv_status == DRV_STS_POWER_ON) {
			pr_warn("[%s] %s already on\n", __func__, drv_name[cal_drvs[i]]);
			osal_unlock_sleepable_lock(&g_conninfra_ctx.core_lock);
			return 0;
		}
	}
	osal_unlock_sleepable_lock(&g_conninfra_ctx.core_lock);

	ret = conninfra_core_power_on(CONNDRV_TYPE_BT);
	if (ret) {
		pr_err("BT power on fail during pre_cal");
		return -1;
	}
	ret = conninfra_core_power_on(CONNDRV_TYPE_WIFI);
	if (ret) {
		pr_err("WIFI power on fail during pre_cal");
		conninfra_core_power_off(CONNDRV_TYPE_BT);
		return -2;
	}

	do_gettimeofday(&begin);

	/* power on subsys */
	atomic_set(&g_conninfra_ctx.pre_cal_state, 0);
	sema_init(&g_conninfra_ctx.pre_cal_sema, 1);

	for (i = 0; i < CAL_DRV_COUNT; i++) {
		drv_inst = &g_conninfra_ctx.drv_inst[cal_drvs[i]];
		ret = msg_thread_send_1(&drv_inst->msg_ctx,
				INFRA_SUBDRV_OPID_CAL_PWR_ON, cal_drvs[i]);
		if (ret)
			pr_warn("driver [%d] power on fail\n", cal_drvs[i]);
	}

	while (atomic_read(&g_conninfra_ctx.pre_cal_state) != pre_cal_done_state) {
		ret = down_timeout(&g_conninfra_ctx.pre_cal_sema, msecs_to_jiffies(CONNINFRA_PRE_CAL_TIMEOUT));
		if (ret == 0)
			continue;
		cur_state = atomic_read(&g_conninfra_ctx.pre_cal_state);
		pr_info("[pre_cal] cur state =[%d]", cur_state);
		if ((cur_state & (0x1 << CONNDRV_TYPE_BT)) == 0) {
			pr_info("[pre_cal] BT pwr_on callback is not back");
			drv_inst = &g_conninfra_ctx.drv_inst[CONNDRV_TYPE_BT];
			osal_thread_show_stack(&drv_inst->msg_ctx.thread);
		}
		if ((cur_state & (0x1 << CONNDRV_TYPE_WIFI)) == 0) {
			pr_info("[pre_cal] WIFI pwr_on callback is not back");
			drv_inst = &g_conninfra_ctx.drv_inst[CONNDRV_TYPE_WIFI];
			osal_thread_show_stack(&drv_inst->msg_ctx.thread);
		}
	}
	pr_info("[pre_cal] >>>>>>> power on DONE!!");

	do_gettimeofday(&bt_cal_begin);

	/* Do Calibration */
	drv_inst = &g_conninfra_ctx.drv_inst[CONNDRV_TYPE_BT];
	bt_cal_ret = msg_thread_send_wait_1(&drv_inst->msg_ctx,
			INFRA_SUBDRV_OPID_CAL_DO_CAL, 0, CONNDRV_TYPE_BT);

	pr_info("[pre_cal] driver [%s] calibration %s, ret=[%d]\n", drv_name[CONNDRV_TYPE_BT],
			(bt_cal_ret == CONNINFRA_CB_RET_CAL_FAIL_POWER_OFF ||
			bt_cal_ret == CONNINFRA_CB_RET_CAL_FAIL_POWER_ON) ? "fail" : "success",
			bt_cal_ret);

	if (bt_cal_ret == CONNINFRA_CB_RET_CAL_PASS_POWER_OFF ||
		bt_cal_ret == CONNINFRA_CB_RET_CAL_FAIL_POWER_OFF)
		conninfra_core_power_off(CONNDRV_TYPE_BT);

	pr_info("[pre_cal] >>>>>>>> BT do cal done");

	do_gettimeofday(&wf_cal_begin);

	drv_inst = &g_conninfra_ctx.drv_inst[CONNDRV_TYPE_WIFI];
	wf_cal_ret = msg_thread_send_wait_1(&drv_inst->msg_ctx,
			INFRA_SUBDRV_OPID_CAL_DO_CAL, 0, CONNDRV_TYPE_WIFI);

	pr_info("[pre_cal] driver [%s] calibration %s, ret=[%d]\n", drv_name[CONNDRV_TYPE_WIFI],
			(wf_cal_ret == CONNINFRA_CB_RET_CAL_FAIL_POWER_OFF ||
			wf_cal_ret == CONNINFRA_CB_RET_CAL_FAIL_POWER_ON) ? "fail" : "success",
			wf_cal_ret);

	if (wf_cal_ret == CONNINFRA_CB_RET_CAL_PASS_POWER_OFF ||
		wf_cal_ret == CONNINFRA_CB_RET_CAL_FAIL_POWER_OFF)
		conninfra_core_power_off(CONNDRV_TYPE_WIFI);

	pr_info(">>>>>>>> WF do cal done");

	do_gettimeofday(&end);

	pr_info("[pre_cal] summary pwr=[%lu] bt_cal=[%d][%lu] wf_cal=[%d][%lu]",
			timeval_to_ms(&begin, &bt_cal_begin),
			bt_cal_ret, timeval_to_ms(&bt_cal_begin, &wf_cal_begin),
			wf_cal_ret, timeval_to_ms(&wf_cal_begin, &end));

	return 0;
}


static int opfunc_therm_ctrl(struct msg_op_data *op)
{
	int ret = -1;
	int *data_ptr = (int*)op->op_data[0];

	if (g_conninfra_ctx.infra_drv_status != DRV_STS_POWER_ON) {
		*data_ptr = 0;
		return 0;
	}

	if (data_ptr)
		ret = consys_hw_therm_query(data_ptr);
	return ret;
}


static int opfunc_rfspi_read(struct msg_op_data *op)
{
	int ret = 0;
	unsigned int data = 0;
	unsigned int* data_pt = (unsigned int*)op->op_data[2];

	ret = osal_lock_sleepable_lock(&g_conninfra_ctx.core_lock);
	if (ret) {
		pr_err("core_lock fail!!\n");
		return CONNINFRA_SPI_OP_FAIL;
	}

	if (g_conninfra_ctx.infra_drv_status != DRV_STS_POWER_ON) {
		pr_err("Connsys didn't power on\n");
		ret = CONNINFRA_SPI_OP_FAIL;
		goto err;
	}
	if (consys_hw_reg_readable() == 0) {
		pr_err("connsys reg not readable\n");
		ret = CONNINFRA_SPI_OP_FAIL;
		goto err;
	}
	/* DO read spi */
	ret = consys_hw_spi_read(op->op_data[0], op->op_data[1], &data);
	if (data_pt)
		*(data_pt) = data;
err:
	osal_unlock_sleepable_lock(&g_conninfra_ctx.core_lock);
	return ret;
}

static int opfunc_rfspi_write(struct msg_op_data *op)
{
	int ret = 0;

	ret = osal_lock_sleepable_lock(&g_conninfra_ctx.core_lock);
	if (ret) {
		pr_err("core_lock fail!!\n");
		return CONNINFRA_SPI_OP_FAIL;
	}

	if (g_conninfra_ctx.infra_drv_status != DRV_STS_POWER_ON) {
		pr_err("Connsys didn't power on\n");
		ret = CONNINFRA_SPI_OP_FAIL;
		goto err;
	}
	if (consys_hw_reg_readable() == 0) {
		pr_err("connsys reg not readable\n");
		ret = CONNINFRA_SPI_OP_FAIL;
		goto err;
	}
	/* DO spi write */
	ret = consys_hw_spi_write(op->op_data[0], op->op_data[1], op->op_data[2]);
err:
	osal_unlock_sleepable_lock(&g_conninfra_ctx.core_lock);
	return ret;
}

static int opfunc_adie_top_ck_en_on(struct msg_op_data *op)
{
	int ret = 0;
	unsigned int type = op->op_data[0];

	if (type >= CONNSYS_ADIE_CTL_MAX) {
		pr_err("wrong parameter %d\n", type);
		return -EINVAL;
	}

	ret = osal_lock_sleepable_lock(&g_conninfra_ctx.core_lock);
	if (ret) {
		pr_err("core_lock fail!!\n");
		ret = -1;
		goto err;
	}

	if (g_conninfra_ctx.infra_drv_status != DRV_STS_POWER_ON) {
		pr_err("Connsys didn't power on\n");
		ret = -2;
		goto err;
	}

	ret = consys_hw_adie_top_ck_en_on(type);

err:
	osal_unlock_sleepable_lock(&g_conninfra_ctx.core_lock);
	return ret;
}


static int opfunc_adie_top_ck_en_off(struct msg_op_data *op)
{
	int ret = 0;
	unsigned int type = op->op_data[0];

	if (type >= CONNSYS_ADIE_CTL_MAX) {
		pr_err("wrong parameter %d\n", type);
		return -EINVAL;
	}

	ret = osal_lock_sleepable_lock(&g_conninfra_ctx.core_lock);
	if (ret) {
		pr_err("core_lock fail!!\n");
		ret = -1;
		goto err;
	}
	if (g_conninfra_ctx.infra_drv_status != DRV_STS_POWER_ON) {
		pr_err("Connsys didn't power on\n");
		ret = -2;
		goto err;
	}

	ret = consys_hw_adie_top_ck_en_off(type);
err:
	osal_unlock_sleepable_lock(&g_conninfra_ctx.core_lock);
	return ret;
}

static int opfunc_spi_clock_switch(struct msg_op_data *op)
{
	int ret = 0;
	unsigned int type = op->op_data[0];

	if (type >= CONNSYS_SPI_SPEED_MAX) {
		pr_err("wrong parameter %d\n", type);
		return -EINVAL;
	}

	ret = osal_lock_sleepable_lock(&g_conninfra_ctx.core_lock);
	if (ret) {
		pr_err("core_lock fail!!\n");
		ret = -2;
		goto err;
	}
	if (g_conninfra_ctx.infra_drv_status != DRV_STS_POWER_ON) {
		pr_err("Connsys didn't power on\n");
		ret = -2;
		goto err;
	}

	ret = consys_hw_spi_clock_switch(type);
err:
	osal_unlock_sleepable_lock(&g_conninfra_ctx.core_lock);
	return ret;
}


static int opfunc_clock_fail_dump(struct msg_op_data *op)
{
	consys_hw_clock_fail_dump();
	return 0;
}


static int opfunc_pre_cal_prepare(struct msg_op_data *op)
{
	int ret, rst_status;
	unsigned long flag;
	struct pre_cal_info *cal_info = &g_conninfra_ctx.cal_info;
	struct subsys_drv_inst *bt_drv = &g_conninfra_ctx.drv_inst[CONNDRV_TYPE_BT];
	struct subsys_drv_inst *wifi_drv = &g_conninfra_ctx.drv_inst[CONNDRV_TYPE_WIFI];
	enum pre_cal_status cur_status;

	spin_lock_irqsave(&g_conninfra_ctx.infra_lock, flag);

	if (bt_drv->ops_cb.pre_cal_cb.do_cal_cb == NULL ||
		wifi_drv->ops_cb.pre_cal_cb.do_cal_cb == NULL) {
		pr_info("[%s] [pre_cal] [%p][%p]", __func__,
			bt_drv->ops_cb.pre_cal_cb.do_cal_cb,
			wifi_drv->ops_cb.pre_cal_cb.do_cal_cb);
		spin_unlock_irqrestore(&g_conninfra_ctx.infra_lock, flag);
		return 0;
	}
	spin_unlock_irqrestore(&g_conninfra_ctx.infra_lock, flag);

	spin_lock_irqsave(&g_conninfra_ctx.rst_lock, flag);
	rst_status = g_conninfra_ctx.rst_status;
	spin_unlock_irqrestore(&g_conninfra_ctx.rst_lock, flag);

	if (rst_status > CHIP_RST_NONE) {
		pr_info("rst is ongoing, skip pre_cal");
		return 0;
	}

	/* non-zero means lock got, zero means not */
	ret = osal_trylock_sleepable_lock(&cal_info->pre_cal_lock);

	if (ret) {
		cur_status = cal_info->status;

		if ((cur_status == PRE_CAL_NOT_INIT || cur_status == PRE_CAL_NEED_RESCHEDULE) &&
			bt_drv->drv_status == DRV_STS_POWER_OFF &&
			wifi_drv->drv_status == DRV_STS_POWER_OFF) {
			cal_info->status = PRE_CAL_SCHEDULED;
			cal_info->caller = op->op_data[0];
			pr_info("[pre_cal] BT&WIFI is off, schedule pre-cal from status=[%d] to new status[%d]\n",
				cur_status, cal_info->status);
			schedule_work(&cal_info->pre_cal_work);
		} else {
			pr_info("[%s] [pre_cal] bt=[%d] wf=[%d] status=[%d]", __func__,
				bt_drv->drv_status, wifi_drv->drv_status, cur_status);
		}
		osal_unlock_sleepable_lock(&cal_info->pre_cal_lock);
	}

	return 0;
}

static int opfunc_pre_cal_check(struct msg_op_data *op)
{
	int ret;
	struct pre_cal_info *cal_info = &g_conninfra_ctx.cal_info;
	struct subsys_drv_inst *bt_drv = &g_conninfra_ctx.drv_inst[CONNDRV_TYPE_BT];
	struct subsys_drv_inst *wifi_drv = &g_conninfra_ctx.drv_inst[CONNDRV_TYPE_WIFI];
	enum pre_cal_status cur_status;

	/* non-zero means lock got, zero means not */
	ret = osal_trylock_sleepable_lock(&cal_info->pre_cal_lock);
	if (ret) {
		cur_status = cal_info->status;

		pr_info("[%s] [pre_cal] bt=[%d] wf=[%d] status=[%d]", __func__,
			bt_drv->drv_status, wifi_drv->drv_status,
			cur_status);
		if (cur_status == PRE_CAL_DONE &&
			bt_drv->drv_status == DRV_STS_POWER_OFF &&
			wifi_drv->drv_status == DRV_STS_POWER_OFF) {
			pr_info("[pre_cal] reset pre-cal");
			cal_info->status = PRE_CAL_NEED_RESCHEDULE;
		}
		osal_unlock_sleepable_lock(&cal_info->pre_cal_lock);
	}
	return 0;
}


static int opfunc_force_conninfra_wakeup(struct msg_op_data *op)
{
	int ret;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = osal_lock_sleepable_lock(&infra_ctx->core_lock);
	if (ret) {
		pr_err("core_lock fail!!");
		return ret;
	}

	/* check if conninfra already on */
	if (g_conninfra_ctx.infra_drv_status != DRV_STS_POWER_ON) {
		ret = -1;
		goto err;
	}

	ret = consys_hw_force_conninfra_wakeup();
	if (ret)
		pr_err("force conninfra wakeup fail");

err:
	osal_unlock_sleepable_lock(&infra_ctx->core_lock);
	return ret;
}

static int opfunc_force_conninfra_sleep(struct msg_op_data *op)
{
	int ret;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = osal_lock_sleepable_lock(&infra_ctx->core_lock);
	if (ret) {
		pr_err("core_lock fail!!");
		return ret;
	}

	/* check if conninfra already on */
	if (g_conninfra_ctx.infra_drv_status != DRV_STS_POWER_ON) {
		ret = -1;
		goto err;
	}

	ret = consys_hw_force_conninfra_sleep();
	if (ret)
		pr_err("force conninfra sleep fail");

err:
	osal_unlock_sleepable_lock(&infra_ctx->core_lock);
	return ret;
}


static int opfunc_dump_power_state(struct msg_op_data *op)
{
	int ret;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = osal_lock_sleepable_lock(&infra_ctx->core_lock);
	if (ret) {
		pr_err("core_lock fail!!");
		return ret;
	}

	/* check if conninfra already on */
	if (g_conninfra_ctx.infra_drv_status != DRV_STS_POWER_ON) {
		ret = -1;
		goto err;
	}

	ret = consys_hw_dump_power_state();
	if (ret)
		pr_err("dump power state fail");

err:
	osal_unlock_sleepable_lock(&infra_ctx->core_lock);
	return ret;

}

static int opfunc_subdrv_pre_reset(struct msg_op_data *op)
{
	int ret, cur_rst_state;
	unsigned int drv_type = op->op_data[0];
	struct subsys_drv_inst *drv_inst;


	/* TODO: should be locked, to avoid cb was reset */
	drv_inst = &g_conninfra_ctx.drv_inst[drv_type];
	if (/*drv_inst->drv_status == DRV_ST_POWER_ON &&*/
			drv_inst->ops_cb.rst_cb.pre_whole_chip_rst) {

		ret = drv_inst->ops_cb.rst_cb.pre_whole_chip_rst(g_conninfra_ctx.trg_drv,
					g_conninfra_ctx.trg_reason);
		if (ret)
			pr_err("[%s] fail [%d]", __func__, ret);
	}

	atomic_add(0x1 << drv_type, &g_conninfra_ctx.rst_state);
	cur_rst_state = atomic_read(&g_conninfra_ctx.rst_state);

	pr_info("[%s] rst_state=[%d]", drv_thread_name[drv_type], cur_rst_state);

	up(&g_conninfra_ctx.rst_sema);
	return 0;
}

static int opfunc_subdrv_post_reset(struct msg_op_data *op)
{
	int ret;
	unsigned int drv_type = op->op_data[0];
	struct subsys_drv_inst *drv_inst;

	/* TODO: should be locked, to avoid cb was reset */
	drv_inst = &g_conninfra_ctx.drv_inst[drv_type];
	if (/*drv_inst->drv_status == DRV_ST_POWER_ON &&*/
			drv_inst->ops_cb.rst_cb.post_whole_chip_rst) {
		ret = drv_inst->ops_cb.rst_cb.post_whole_chip_rst();
		if (ret)
			pr_warn("[%s] fail [%d]", __func__, ret);
	}

	atomic_add(0x1 << drv_type, &g_conninfra_ctx.rst_state);
	up(&g_conninfra_ctx.rst_sema);
	return 0;
}

static int opfunc_subdrv_cal_pwr_on(struct msg_op_data *op)
{
	int ret;
	unsigned int drv_type = op->op_data[0];
	struct subsys_drv_inst *drv_inst;

	pr_info("[%s] drv=[%s]", __func__, drv_thread_name[drv_type]);

	/* TODO: should be locked, to avoid cb was reset */
	drv_inst = &g_conninfra_ctx.drv_inst[drv_type];
	if (/*drv_inst->drv_status == DRV_ST_POWER_ON &&*/
			drv_inst->ops_cb.pre_cal_cb.pwr_on_cb) {
		ret = drv_inst->ops_cb.pre_cal_cb.pwr_on_cb();
		if (ret)
			pr_warn("[%s] fail [%d]", __func__, ret);
	}

	atomic_add(0x1 << drv_type, &g_conninfra_ctx.pre_cal_state);
	up(&g_conninfra_ctx.pre_cal_sema);

	pr_info("[pre_cal][%s] [%s] DONE", __func__, drv_thread_name[drv_type]);
	return 0;
}

static int opfunc_subdrv_cal_do_cal(struct msg_op_data *op)
{
	int ret = 0;
	unsigned int drv_type = op->op_data[0];
	struct subsys_drv_inst *drv_inst;

	pr_info("[%s] drv=[%s]", __func__, drv_thread_name[drv_type]);

	drv_inst = &g_conninfra_ctx.drv_inst[drv_type];
	if (/*drv_inst->drv_status == DRV_ST_POWER_ON &&*/
			drv_inst->ops_cb.pre_cal_cb.do_cal_cb) {
		ret = drv_inst->ops_cb.pre_cal_cb.do_cal_cb();
		if (ret)
			pr_warn("[%s] fail [%d]", __func__, ret);
	}

	pr_info("[pre_cal][%s] [%s] DONE", __func__, drv_thread_name[drv_type]);
	return ret;
}

static int opfunc_subdrv_therm_ctrl(struct msg_op_data *op)
{
	return 0;
}


/*
 * CONNINFRA API
 */
int conninfra_core_power_on(enum consys_drv_type type)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = msg_thread_send_wait_1(&infra_ctx->msg_ctx,
				CONNINFRA_OPID_PWR_ON, 0, type);
	if (ret) {
		pr_err("[%s] fail, ret = %d\n", __func__, ret);
		return -1;
	}
	return 0;
}

int conninfra_core_power_off(enum consys_drv_type type)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = msg_thread_send_wait_1(&infra_ctx->msg_ctx,
				CONNINFRA_OPID_PWR_OFF, 0, type);
	if (ret) {
		pr_err("[%s] send msg fail, ret = %d\n", __func__, ret);
		return -1;
	}
	return 0;
}

int conninfra_core_pre_cal_start(void)
{
	int ret = 0;
	bool skip = false;
	enum pre_cal_caller caller;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;
	struct pre_cal_info *cal_info = &infra_ctx->cal_info;

	ret = osal_lock_sleepable_lock(&cal_info->pre_cal_lock);
	if (ret) {
		pr_err("[%s] get lock fail, ret = %d\n",
			__func__, ret);
		return -1;
	}

	caller = cal_info->caller;
	pr_info("[%s] [pre_cal] Caller = %u", __func__, caller);

	/* Handle different pre_cal_mode */
	switch (g_pre_cal_mode) {
		case PRE_CAL_ALL_DISABLED:
			pr_info("[%s] [pre_cal] Skip all pre-cal", __func__);
			skip = true;
			cal_info->status = PRE_CAL_DONE;
			break;
		case PRE_CAL_PWR_ON_DISABLED:
			if (caller == PRE_CAL_BY_SUBDRV_REGISTER) {
				pr_info("[%s] [pre_cal] Skip pre-cal triggered by subdrv register", __func__);
				skip = true;
				cal_info->status = PRE_CAL_NOT_INIT;
			}
			break;
		case PRE_CAL_SCREEN_ON_DISABLED:
			if (caller == PRE_CAL_BY_SCREEN_ON) {
				pr_info("[%s] [pre_cal] Skip pre-cal triggered by screen on", __func__);
				skip = true;
				cal_info->status = PRE_CAL_DONE;
			}
			break;
		default:
			pr_info("[%s] [pre_cal] Begin pre-cal, g_pre_cal_mode: %u",
				__func__, g_pre_cal_mode);
			break;
	}

	if (skip) {
		pr_info("[%s] [pre_cal] Reset status to %d", __func__, cal_info->status);
		osal_unlock_sleepable_lock(&cal_info->pre_cal_lock);
		return -2;
	}

	cal_info->status = PRE_CAL_EXECUTING;
	ret = msg_thread_send_wait(&infra_ctx->cb_ctx,
				CONNINFRA_CB_OPID_PRE_CAL, 0);
	if (ret) {
		pr_err("[%s] send msg fail, ret = %d\n", __func__, ret);
	}

	cal_info->status = PRE_CAL_DONE;
	osal_unlock_sleepable_lock(&cal_info->pre_cal_lock);
	return 0;
}

int conninfra_core_screen_on(void)
{
	int ret = 0, rst_status;
	unsigned long flag;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	spin_lock_irqsave(&infra_ctx->rst_lock, flag);
	rst_status = g_conninfra_ctx.rst_status;
	spin_unlock_irqrestore(&infra_ctx->rst_lock, flag);

	if (rst_status > CHIP_RST_NONE) {
		pr_info("rst is ongoing, skip pre_cal");
		return 0;
	}

	ret = msg_thread_send_1(&infra_ctx->msg_ctx,
			CONNINFRA_OPID_PRE_CAL_PREPARE, PRE_CAL_BY_SCREEN_ON);
	if (ret) {
		pr_err("[%s] send msg fail, ret = %d\n", __func__, ret);
		return -1;
	}
	return 0;
}

int conninfra_core_screen_off(void)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = msg_thread_send(&infra_ctx->msg_ctx,
				CONNINFRA_OPID_PRE_CAL_CHECK);
	if (ret) {
		pr_err("[%s] send msg fail, ret = %d\n", __func__, ret);
		return -1;
	}

	return 0;
}

int conninfra_core_reg_readable(void)
{
	int ret = 0, rst_status;
	unsigned long flag;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;


	/* check if in reseting, can not read */
	spin_lock_irqsave(&g_conninfra_ctx.rst_lock, flag);
	rst_status = g_conninfra_ctx.rst_status;
	spin_unlock_irqrestore(&g_conninfra_ctx.rst_lock, flag);

	if (rst_status >= CHIP_RST_RESET &&
		rst_status < CHIP_RST_POST_CB)
		return 0;

	ret = osal_lock_sleepable_lock(&infra_ctx->core_lock);
	if (ret) {
		pr_err("core_lock fail!!");
		return 0;
	}

	if (infra_ctx->infra_drv_status == DRV_STS_POWER_ON)
		ret = consys_hw_reg_readable();
	osal_unlock_sleepable_lock(&infra_ctx->core_lock);

	return ret;
}

int conninfra_core_reg_readable_no_lock(void)
{
	int rst_status;
	unsigned long flag;

	/* check if in reseting, can not read */
	spin_lock_irqsave(&g_conninfra_ctx.rst_lock, flag);
	rst_status = g_conninfra_ctx.rst_status;
	spin_unlock_irqrestore(&g_conninfra_ctx.rst_lock, flag);

	if (rst_status >= CHIP_RST_RESET &&
		rst_status < CHIP_RST_POST_CB)
		return 0;

	return consys_hw_reg_readable();
}

int conninfra_core_is_bus_hang(void)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = osal_lock_sleepable_lock(&infra_ctx->core_lock);
	if (ret) {
		pr_err("core_lock fail!!");
		return 0;
	}

	if (infra_ctx->infra_drv_status == DRV_STS_POWER_ON)
		ret = consys_hw_is_bus_hang();
	osal_unlock_sleepable_lock(&infra_ctx->core_lock);

	return ret;

}

int conninfra_core_is_consys_reg(phys_addr_t addr)
{
	return consys_hw_is_connsys_reg(addr);
}

int conninfra_core_reg_read(unsigned long address, unsigned int *value, unsigned int mask)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = osal_lock_sleepable_lock(&infra_ctx->core_lock);
	if (ret) {
		pr_err("core_lock fail!!");
		return 0;
	}

	if (infra_ctx->infra_drv_status == DRV_STS_POWER_ON) {
		if (consys_reg_mng_is_host_csr(address))
			ret = consys_reg_mng_reg_read(address, value, mask);
		else if (consys_hw_reg_readable())
			ret = consys_reg_mng_reg_read(address, value, mask);
		else
			pr_info("CR (%lx) is not readable\n", address);
	} else
		pr_info("CR (%lx) cannot read. conninfra is off\n", address);

	osal_unlock_sleepable_lock(&infra_ctx->core_lock);
	return ret;
}

int conninfra_core_reg_write(unsigned long address, unsigned int value, unsigned int mask)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = osal_lock_sleepable_lock(&infra_ctx->core_lock);
	if (ret) {
		pr_err("core_lock fail!!");
		return 0;
	}

	if (infra_ctx->infra_drv_status == DRV_STS_POWER_ON) {
		if (consys_reg_mng_is_host_csr(address))
			ret = consys_reg_mng_reg_write(address, value, mask);
		else if (consys_hw_reg_readable())
			ret = consys_reg_mng_reg_write(address, value, mask);
		else
			pr_info("CR (%p) is not readable\n", (void*)address);
	} else
		pr_info("CR (%p) cannot read. conninfra is off\n", (void*)address);

	osal_unlock_sleepable_lock(&infra_ctx->core_lock);
	return ret;

}

int conninfra_core_lock_rst(void)
{
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;
	int ret = 0;
	unsigned long flag;

	spin_lock_irqsave(&infra_ctx->rst_lock, flag);

	ret = infra_ctx->rst_status;
	if (infra_ctx->rst_status > CHIP_RST_NONE &&
		infra_ctx->rst_status < CHIP_RST_DONE) {
		/* do nothing */
	} else {
		infra_ctx->rst_status = CHIP_RST_START;
	}
	spin_unlock_irqrestore(&infra_ctx->rst_lock, flag);

	pr_info("[%s] ret=[%d]", __func__, ret);
	return ret;
}

int conninfra_core_unlock_rst(void)
{
	unsigned long flag;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	spin_lock_irqsave(&infra_ctx->rst_lock, flag);
	infra_ctx->rst_status = CHIP_RST_NONE;
	spin_unlock_irqrestore(&infra_ctx->rst_lock, flag);
	return 0;
}

int conninfra_core_trg_chip_rst(enum consys_drv_type drv, char *reason)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	infra_ctx->trg_drv = drv;
	if (snprintf(infra_ctx->trg_reason, CHIP_RST_REASON_MAX_LEN, "%s", reason) < 0)
		pr_warn("[%s::%d] snprintf error\n", __func__, __LINE__);
	ret = msg_thread_send_1(&infra_ctx->cb_ctx,
				CONNINFRA_CB_OPID_CHIP_RST, drv);
	if (ret) {
		pr_err("[%s] send msg fail, ret = %d", __func__, ret);
		return -1;
	}
	pr_info("trg_reset DONE!");
	return 0;
}

int conninfra_core_thermal_query(int *temp_val)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = msg_thread_send_wait_1(&infra_ctx->msg_ctx,
		CONNINFRA_OPID_THERM_CTRL, 0,
		(size_t) temp_val);
	if (ret) {
		pr_err("send msg fail ret=%d\n", ret);
		return ret;
	}
	pr_info("ret=[%d] temp=[%d]\n", ret, *temp_val);
	return ret;
}

void conninfra_core_clock_fail_dump_cb(void)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = msg_thread_send(&infra_ctx->msg_ctx,
		CONNINFRA_OPID_CLOCK_FAIL_DUMP);
	if (ret)
		pr_err("failed (ret = %d)", ret);
}

static inline char* conninfra_core_spi_subsys_string(enum sys_spi_subsystem subsystem)
{
	static char* subsys_name[] = {
		"SYS_SPI_WF1",
		"SYS_SPI_WF",
		"SYS_SPI_BT",
		"SYS_SPI_FM",
		"SYS_SPI_GPS",
		"SYS_SPI_TOP",
		"SYS_SPI_WF2",
		"SYS_SPI_WF3",
		"SYS_SPI_MAX"
	};

	if (subsystem < 0 || subsystem > SYS_SPI_MAX)
		return "UNKNOWN";

	return subsys_name[subsystem];
}

int conninfra_core_spi_read(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int *data)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;
	size_t data_ptr = (size_t)data;

	ret = msg_thread_send_wait_3(&infra_ctx->msg_ctx,
		CONNINFRA_OPID_RFSPI_READ, 0,
		subsystem, addr, data_ptr);
	if (ret) {
		pr_err("[%s] failed (ret = %d). subsystem=%s addr=%x\n",
			__func__, ret, conninfra_core_spi_subsys_string(subsystem), addr);
		return CONNINFRA_SPI_OP_FAIL;
	}
	return 0;
}

int conninfra_core_spi_write(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int data)
{
	int ret;
	ret = msg_thread_send_wait_3(&(g_conninfra_ctx.msg_ctx), CONNINFRA_OPID_RFSPI_WRITE, 0,
		subsystem, addr, data);
	if (ret) {
		pr_err("[%s] failed (ret = %d). subsystem=%s addr=0x%x data=%d\n",
			__func__, ret, conninfra_core_spi_subsys_string(subsystem), addr, data);
		return CONNINFRA_SPI_OP_FAIL;
	}
	return 0;
}

int conninfra_core_adie_top_ck_en_on(enum consys_adie_ctl_type type)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = msg_thread_send_wait_1(&infra_ctx->msg_ctx,
				CONNINFRA_OPID_ADIE_TOP_CK_EN_ON, 0, type);
	if (ret) {
		pr_err("[%s] fail, ret = %d\n", __func__, ret);
		return -1;
	}
	return 0;
}

int conninfra_core_adie_top_ck_en_off(enum consys_adie_ctl_type type)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = msg_thread_send_wait_1(&infra_ctx->msg_ctx,
				CONNINFRA_OPID_ADIE_TOP_CK_EN_OFF, 0, type);
	if (ret) {
		pr_err("[%s] fail, ret = %d\n", __func__, ret);
		return -1;
	}
	return 0;
}

int conninfra_core_force_conninfra_wakeup(void)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	/* if in conninfra_cored thread */
	if (current == infra_ctx->msg_ctx.thread.pThread)
		return opfunc_force_conninfra_wakeup(NULL);

	ret = msg_thread_send_wait(&infra_ctx->msg_ctx,
				CONNINFRA_OPID_FORCE_CONNINFRA_WAKUP, 0);
	if (ret) {
		pr_err("[%s] fail, ret = %d\n", __func__, ret);
		return -1;
	}
	return 0;
}

int conninfra_core_force_conninfra_sleep(void)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	/* if in conninfra_cored thread */
	if (current == infra_ctx->msg_ctx.thread.pThread)
		return opfunc_force_conninfra_sleep(NULL);

	ret = msg_thread_send_wait(&infra_ctx->msg_ctx,
				CONNINFRA_OPID_FORCE_CONNINFRA_SLEEP, 0);
	if (ret) {
		pr_err("[%s] fail, ret = %d\n", __func__, ret);
		return -1;
	}
	return 0;
}

int conninfra_core_spi_clock_switch(enum connsys_spi_speed_type type)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = msg_thread_send_wait_1(&infra_ctx->msg_ctx,
				CONNINFRA_OPID_SPI_CLOCK_SWITCH, 0, type);
	if (ret) {
		pr_err("[%s] fail, ret = %d\n", __func__, ret);
		return -1;
	}
	return 0;
}

int conninfra_core_subsys_ops_reg(enum consys_drv_type type,
					struct sub_drv_ops_cb *cb)
{
	unsigned long flag;
	struct subsys_drv_inst *drv_inst;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;
	int ret, trigger_pre_cal = 0;

	if (type < CONNDRV_TYPE_BT || type >= CONNDRV_TYPE_MAX)
		return -1;

	spin_lock_irqsave(&g_conninfra_ctx.infra_lock, flag);
	drv_inst = &g_conninfra_ctx.drv_inst[type];
	memcpy(&g_conninfra_ctx.drv_inst[type].ops_cb, cb,
					sizeof(struct sub_drv_ops_cb));

	pr_info("[%s] [pre_cal] type=[%s] cb rst=[%p][%p] pre_cal=[%p][%p], therm=[%p]",
			__func__, drv_name[type],
			cb->rst_cb.pre_whole_chip_rst, cb->rst_cb.post_whole_chip_rst,
			cb->pre_cal_cb.pwr_on_cb, cb->pre_cal_cb.do_cal_cb, cb->thermal_qry);

	pr_info("[%s] [pre_cal] type=[%d] bt=[%p] wf=[%p]", __func__, type,
			infra_ctx->drv_inst[CONNDRV_TYPE_BT].ops_cb.pre_cal_cb.pwr_on_cb,
			infra_ctx->drv_inst[CONNDRV_TYPE_WIFI].ops_cb.pre_cal_cb.pwr_on_cb);

	/* trigger pre-cal if BT and WIFI are registered */
	if (infra_ctx->drv_inst[CONNDRV_TYPE_BT].ops_cb.pre_cal_cb.do_cal_cb != NULL &&
		infra_ctx->drv_inst[CONNDRV_TYPE_WIFI].ops_cb.pre_cal_cb.do_cal_cb != NULL)
		trigger_pre_cal = 1;

	spin_unlock_irqrestore(&g_conninfra_ctx.infra_lock, flag);

	if (trigger_pre_cal) {
		pr_info("[%s] [pre_cal] trigger pre-cal BT/WF are registered", __func__);
		ret = msg_thread_send_1(&infra_ctx->msg_ctx,
				CONNINFRA_OPID_PRE_CAL_PREPARE, PRE_CAL_BY_SUBDRV_REGISTER);
		if (ret)
			pr_err("send pre_cal_prepare msg fail, ret = %d\n", ret);
	}

	return 0;
}

int conninfra_core_subsys_ops_unreg(enum consys_drv_type type)
{
	unsigned long flag;

	if (type < CONNDRV_TYPE_BT || type >= CONNDRV_TYPE_MAX)
		return -1;
	spin_lock_irqsave(&g_conninfra_ctx.infra_lock, flag);
	memset(&g_conninfra_ctx.drv_inst[type].ops_cb, 0,
					sizeof(struct sub_drv_ops_cb));
	spin_unlock_irqrestore(&g_conninfra_ctx.infra_lock, flag);

	return 0;
}

#if ENABLE_PRE_CAL_BLOCKING_CHECK
void conninfra_core_pre_cal_blocking(void)
{
#define BLOCKING_CHECK_MONITOR_THREAD 100
	int ret;
	struct pre_cal_info *cal_info = &g_conninfra_ctx.cal_info;
	struct timeval start, end;
	unsigned long diff;
	static bool ever_pre_cal = false;

	do_gettimeofday(&start);

	/* non-zero means lock got, zero means not */
	while (true) {
		// Handle PRE_CAL_PWR_ON_DISABLED case:
		// 1. Do pre-cal "only once" after bootup if BT or WIFI is default on
		// 2. Use ever_pre_cal to check if the "first" pre-cal is already
		//    triggered. If yes, skip pre-cal
		if (g_pre_cal_mode == PRE_CAL_PWR_ON_DISABLED && !ever_pre_cal) {
			struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

			ret = msg_thread_send_1(&infra_ctx->msg_ctx,
					CONNINFRA_OPID_PRE_CAL_PREPARE, PRE_CAL_BY_SUBDRV_PWR_ON);
			ever_pre_cal = true;
			pr_info("[%s] [pre_cal] Triggered by subdrv power on and set ever_pre_cal to true, result: %d", __func__, ret);
		}

		ret = osal_trylock_sleepable_lock(&cal_info->pre_cal_lock);
		if (ret) {
			if (cal_info->status == PRE_CAL_NOT_INIT ||
					cal_info->status == PRE_CAL_SCHEDULED) {
				pr_info("[%s] [pre_cal] ret=[%d] status=[%d]", __func__, ret, cal_info->status);
				osal_unlock_sleepable_lock(&cal_info->pre_cal_lock);
				osal_sleep_ms(100);
				continue;
			}
			osal_unlock_sleepable_lock(&cal_info->pre_cal_lock);
			break;
		} else {
			pr_info("[%s] [pre_cal] ret=[%d] status=[%d]", __func__, ret, cal_info->status);
			osal_sleep_ms(100);
		}
	}
	do_gettimeofday(&end);

	diff = timeval_to_ms(&start, &end);
	if (diff > BLOCKING_CHECK_MONITOR_THREAD)
		pr_info("blocking spent [%lu]", diff);
}
#endif


static void _conninfra_core_update_rst_status(enum chip_rst_status status)
{
	unsigned long flag;

	spin_lock_irqsave(&g_conninfra_ctx.rst_lock, flag);
	g_conninfra_ctx.rst_status = status;
	spin_unlock_irqrestore(&g_conninfra_ctx.rst_lock, flag);
}


int conninfra_core_is_rst_locking(void)
{
	unsigned long flag;
	int ret = 0;

	spin_lock_irqsave(&g_conninfra_ctx.rst_lock, flag);

	if (g_conninfra_ctx.rst_status > CHIP_RST_NONE &&
		g_conninfra_ctx.rst_status < CHIP_RST_POST_CB)
		ret = 1;
	spin_unlock_irqrestore(&g_conninfra_ctx.rst_lock, flag);
	return ret;
}

static void conninfra_core_pre_cal_work_handler(struct work_struct *work)
{
	int ret;

	/* if fail, do we need re-try? */
	ret = conninfra_core_pre_cal_start();
	pr_info("[%s] [pre_cal][ret=%d] -----------", __func__, ret);
}


int conninfra_core_dump_power_state(void)
{
	int ret = 0;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	ret = msg_thread_send(&infra_ctx->msg_ctx,
				CONNINFRA_OPID_DUMP_POWER_STATE);
	if (ret) {
		pr_err("[%s] fail, ret = %d\n", __func__, ret);
		return -1;
	}
	return 0;

}

void conninfra_core_config_setup(void)
{
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;
	int ret;

	ret = osal_lock_sleepable_lock(&infra_ctx->core_lock);
	if (ret) {
		pr_err("[%s] core_lock fail!!\n", __func__);
		return;
	}

	if (infra_ctx->infra_drv_status == DRV_STS_POWER_ON)
		consys_hw_config_setup();

	osal_unlock_sleepable_lock(&infra_ctx->core_lock);
}

int conninfra_core_pmic_event_cb(unsigned int id, unsigned int event)
{
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;
	int ret;

	if (conninfra_core_is_rst_locking()) {
		return 0;
	}

	ret = osal_lock_sleepable_lock(&infra_ctx->core_lock);
	if (ret) {
		pr_err("[%s] core_lock fail!!\n", __func__);
		return 0;
	}

	if (infra_ctx->infra_drv_status == DRV_STS_POWER_ON)
		consys_hw_pmic_event_cb(id, event);

	osal_unlock_sleepable_lock(&infra_ctx->core_lock);

	return 0;
}

int conninfra_core_bus_clock_ctrl(enum consys_drv_type drv_type, unsigned int bus_clock, int status)
{
	int ret = -1, rst_status;
	unsigned long flag;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	/* check if in reseting, can not read */
	spin_lock_irqsave(&g_conninfra_ctx.rst_lock, flag);
	rst_status = g_conninfra_ctx.rst_status;
	spin_unlock_irqrestore(&g_conninfra_ctx.rst_lock, flag);

	if (rst_status >= CHIP_RST_RESET &&
	    rst_status < CHIP_RST_POST_CB)
		return -1;

	ret = osal_lock_sleepable_lock(&infra_ctx->core_lock);
	if (ret) {
		pr_err("[%s] core_lock fail!!", __func__);
		return -1;
	}

	if (infra_ctx->infra_drv_status == DRV_STS_POWER_ON)
		ret = consys_hw_bus_clock_ctrl(drv_type, bus_clock, status);
	osal_unlock_sleepable_lock(&infra_ctx->core_lock);

	return ret;
}

int conninfra_core_init(void)
{
	int ret = 0, i;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	// Get pre-cal mode
	const struct conninfra_conf *conf = NULL;
	conf = conninfra_conf_get_cfg();
	if (conf != NULL) {
		g_pre_cal_mode = conf->pre_cal_mode;
	}
	pr_info("[%s] [pre_cal] Init g_pre_cal_mode = %u", __func__, g_pre_cal_mode);

	osal_memset(&g_conninfra_ctx, 0, sizeof(g_conninfra_ctx));

	reset_chip_rst_trg_data();

	spin_lock_init(&infra_ctx->infra_lock);
	osal_sleepable_lock_init(&infra_ctx->core_lock);
	spin_lock_init(&infra_ctx->rst_lock);


	ret = msg_thread_init(&infra_ctx->msg_ctx, "conninfra_cored",
				conninfra_core_opfunc, CONNINFRA_OPID_MAX);
	if (ret) {
		pr_err("msg_thread init fail(%d)\n", ret);
		return -1;
	}

	ret = msg_thread_init(&infra_ctx->cb_ctx, "conninfra_cb",
                               conninfra_core_cb_opfunc, CONNINFRA_CB_OPID_MAX);
	if (ret) {
		pr_err("callback msg thread init fail(%d)\n", ret);
		return -1;
	}
	/* init subsys drv state */
	for (i = 0; i < CONNDRV_TYPE_MAX; i++) {
		ret += msg_thread_init(&infra_ctx->drv_inst[i].msg_ctx,
				drv_thread_name[i], infra_subdrv_opfunc,
				INFRA_SUBDRV_OPID_MAX);
	}

	if (ret) {
		pr_err("subsys callback thread init fail.\n");
		return -1;
	}

	INIT_WORK(&infra_ctx->cal_info.pre_cal_work, conninfra_core_pre_cal_work_handler);
	osal_sleepable_lock_init(&infra_ctx->cal_info.pre_cal_lock);

	return ret;
}


int conninfra_core_deinit(void)
{
	int ret, i;
	struct conninfra_ctx *infra_ctx = &g_conninfra_ctx;

	osal_sleepable_lock_deinit(&infra_ctx->cal_info.pre_cal_lock);

	for (i = 0; i < CONNDRV_TYPE_MAX; i++) {
		ret = msg_thread_deinit(&infra_ctx->drv_inst[i].msg_ctx);
		if (ret)
			pr_warn("subdrv [%d] msg_thread deinit fail (%d)\n",
						i, ret);
	}

	ret = msg_thread_deinit(&infra_ctx->msg_ctx);
	if (ret) {
		pr_err("msg_thread_deinit fail(%d)\n", ret);
		return -1;
	}

	osal_sleepable_lock_deinit(&infra_ctx->core_lock);
	//osal_sleepable_lock_deinit(&infra_ctx->rst_lock);

	return 0;
}



