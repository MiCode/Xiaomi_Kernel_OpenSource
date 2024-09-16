/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "fm_main.h"
#include "fm_err.h"
#include "fm_reg_utils.h"
/* #include "fm_cust_cfg.h" */
#include "fm_cmd.h"
#include "fm_reg_utils.h"

/* fm main data structure */
static struct fm *g_fm_struct;
/* we must get low level interface first, when add a new chip, the main effort is this interface */
static struct fm_lowlevel_ops fm_low_ops;

/* mutex for char device ops */
static struct fm_lock *fm_ops_lock;
/* mutex for RDS parsing and read result */
static struct fm_lock *fm_read_lock;
/* for get rds block counter */
static struct fm_lock *fm_rds_cnt;
/* mutex for fm timer, RDS reset */
static struct fm_lock *fm_timer_lock;
static struct fm_lock *fm_rxtx_lock;	/* protect FM RX TX mode switch */
static struct fm_lock *fm_rtc_mutex;	/* protect FM GPS RTC drift info */

static struct fm_timer *fm_timer_sys;

#if (FM_INVALID_CHAN_NOISE_REDUCING)
static struct fm_timer *fm_cqi_check_timer;
#endif

static bool scan_stop_flag; /* false */
static struct fm_gps_rtc_info gps_rtc_info;
static bool g_fm_stat[3] = {
	false,		/* RX power */
	false,		/* TX power */
	false,		/* TX scan */
};

/* chipid porting table */
static struct fm_chip_mapping fm_support_chip_array[] = {
{ 0x6572, 0x6627, FM_AD_DIE_CHIP },
{ 0x6582, 0x6627, FM_AD_DIE_CHIP },
{ 0x6592, 0x6627, FM_AD_DIE_CHIP },
{ 0x8127, 0x6627, FM_AD_DIE_CHIP },
{ 0x6571, 0x6627, FM_AD_DIE_CHIP },
{ 0x6752, 0x6627, FM_AD_DIE_CHIP },
{ 0x0321, 0x6627, FM_AD_DIE_CHIP },
{ 0x0335, 0x6627, FM_AD_DIE_CHIP },
{ 0x0337, 0x6627, FM_AD_DIE_CHIP },
{ 0x6735, 0x6627, FM_AD_DIE_CHIP },
{ 0x8163, 0x6627, FM_AD_DIE_CHIP },
{ 0x6755, 0x6627, FM_AD_DIE_CHIP },
{ 0x0326, 0x6627, FM_AD_DIE_CHIP },
{ 0x6570, 0x0633, FM_SOC_CHIP    },
{ 0x6580, 0x6580, FM_SOC_CHIP    },
{ 0x6630, 0x6630, FM_COMBO_CHIP  },
{ 0x6797, 0x6631, FM_AD_DIE_CHIP },
{ 0x6759, 0x6631, FM_AD_DIE_CHIP },
{ 0x6758, 0x6631, FM_AD_DIE_CHIP },
{ 0x6632, 0x6632, FM_COMBO_CHIP  },
{ 0x6757, 0x6627, FM_AD_DIE_CHIP },
{ 0x6763, 0x6627, FM_AD_DIE_CHIP },
{ 0x6765, 0x6631, FM_AD_DIE_CHIP },
{ 0x6761, 0x6631, FM_AD_DIE_CHIP },
{ 0x6739, 0x6627, FM_AD_DIE_CHIP },
{ 0x3967, 0x6631, FM_AD_DIE_CHIP },
{ 0x6771, 0x6631, FM_AD_DIE_CHIP },
{ 0x6775, 0x6631, FM_AD_DIE_CHIP },
{ 0x6768, 0x6631, FM_AD_DIE_CHIP },
{ 0x6779, 0x6635, FM_AD_DIE_CHIP },
};

unsigned char top_index;

/* RDS reset related functions */
static unsigned short fm_cur_freq_get(void);
static signed int fm_cur_freq_set(unsigned short new_freq);
static enum fm_op_state fm_op_state_get(struct fm *fmp);
static enum fm_op_state fm_op_state_set(struct fm *fmp, enum fm_op_state sta);
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
static void fm_timer_func(struct timer_list *timer);
#else
static void fm_timer_func(unsigned long data);
#endif
static void fm_enable_rds_BlerCheck(struct fm *fm);
static void fm_disable_rds_BlerCheck(void);
static void fm_rds_reset_work_func(struct work_struct *work);
/* when interrupt be triggered by FM chip, fm_eint_handler will first be executed */
/* then fm_eint_handler will schedule fm_eint_work_func to run */
static void fm_eint_handler(void);
static void fm_eint_work_func(struct work_struct *work);
static signed int pwrdown_flow(struct fm *fm);

static unsigned short fm_cur_freq_get(void)
{
	return g_fm_struct ? g_fm_struct->cur_freq : 0;
}

static signed int fm_cur_freq_set(unsigned short new_freq)
{
	if (g_fm_struct)
		g_fm_struct->cur_freq = new_freq;

	return 0;
}

static signed int fm_projectid_get(void)
{
	return g_fm_struct ? g_fm_struct->projectid : 0;
}

static enum fm_op_state fm_op_state_get(struct fm *fmp)
{
	if (fmp) {
		WCN_DBG(FM_DBG | MAIN, "op state get %d\n", fmp->op_sta);
		return fmp->op_sta;
	}

	WCN_DBG(FM_ERR | MAIN, "op state get para error\n");
	return FM_STA_UNKNOWN;
}

static enum fm_op_state fm_op_state_set(struct fm *fmp, enum fm_op_state sta)
{
	if (fmp && (sta < FM_STA_MAX)) {
		fmp->op_sta = sta;
		WCN_DBG(FM_DBG | MAIN, "op state set to %d\n", sta);
		return fmp->op_sta;
	}

	WCN_DBG(FM_ERR | MAIN, "op state set para error, %d\n", sta);
	return FM_STA_UNKNOWN;
}

enum fm_pwr_state fm_pwr_state_get(struct fm *fmp)
{
	if (fmp) {
		WCN_DBG(FM_DBG | MAIN, "pwr state get %d\n", fmp->pwr_sta);
		return fmp->pwr_sta;
	}

	WCN_DBG(FM_ERR | MAIN, "pwr state get para error\n");
	return FM_PWR_MAX;
}

enum fm_pwr_state fm_pwr_state_set(struct fm *fmp, enum fm_pwr_state sta)
{
	if (fmp && (sta < FM_PWR_MAX)) {
		fmp->pwr_sta = sta;
		WCN_DBG(FM_DBG | MAIN, "pwr state set to %d\n", sta);
		return fmp->pwr_sta;
	}

	WCN_DBG(FM_ERR | MAIN, "pwr state set para error, %d\n", sta);
	return FM_PWR_MAX;
}

signed int fm_set_stat(struct fm *fmp, int which, bool stat)
{
	signed int ret = 0;

	if (fmp == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if (which >= 0 && which < ARRAY_SIZE(g_fm_stat)) {
		g_fm_stat[which] = stat;
		WCN_DBG(FM_DBG | MAIN, "fm set stat object=%d, stat=%d\n", which, stat);
	} else {
		ret = -1;
		WCN_DBG(FM_ERR | MAIN, "fm set stat error, object=%d, stat=%d\n", which, stat);
	}

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_get_stat(struct fm *fmp, int which, bool *stat)
{
	signed int ret = 0;

	if (fmp == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (stat == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if (which >= 0 && which < ARRAY_SIZE(g_fm_stat)) {
		*stat = g_fm_stat[which];
		WCN_DBG(FM_DBG | MAIN, "fm get stat object=%d, stat=%d\n", which, *stat);
	} else {
		ret = -1;
		WCN_DBG(FM_ERR | MAIN, "fm get stat error, object=%d\n", which);
	}

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

static signed int subsys_rst_state = FM_SUBSYS_RST_OFF;

signed int fm_sys_state_get(struct fm *fmp)
{
	return subsys_rst_state;
}

signed int fm_sys_state_set(struct fm *fmp, signed int sta)
{
	if ((sta >= FM_SUBSYS_RST_OFF) && (sta < FM_SUBSYS_RST_MAX)) {
		WCN_DBG(FM_NTC | MAIN, "sys state set from %d to %d\n", subsys_rst_state, sta);
		subsys_rst_state = sta;
	} else {
		WCN_DBG(FM_ERR | MAIN, "sys state set para error, %d\n", sta);
	}

	return subsys_rst_state;
}

signed int fm_subsys_reset(struct fm *fm)
{
	/* check if we are resetting */
	if (fm_sys_state_get(fm) != FM_SUBSYS_RST_OFF) {
		WCN_DBG(FM_NTC | MAIN, "subsys reset is ongoing\n");
		goto out;
	}

	if (fm == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	fm->timer_wkthd->add_work(fm->timer_wkthd, fm->rst_wk);

out:
	return 0;
}

signed int fm_wholechip_rst_cb(signed int sta)
{
	struct fm *fm = g_fm_struct;

	if (!fm)
		return 0;

	if (sta == 1) {
		if (fm_sys_state_get(fm) == FM_SUBSYS_RST_OFF)
			fm_sys_state_set(fm, FM_SUBSYS_RST_START);
	} else if (sta == 2) {
		if (fm_sys_state_get(fm) == FM_SUBSYS_RST_START)
			fm_sys_state_set(fm, FM_SUBSYS_RST_OFF);
	} else
		fm->timer_wkthd->add_work(fm->timer_wkthd, fm->rst_wk);

	return 0;
}

static signed int fm_which_chip(unsigned short chipid, enum fm_cfg_chip_type *type)
{
	signed short fm_chip = -1;
	signed short i = 0;

	if (fm_wcn_ops.ei.get_get_adie) {
		fm_chip = (signed short)fm_wcn_ops.ei.get_get_adie();
		if (fm_chip == 0x6631 || fm_chip == 0x6635) {
			if (type)
				*type = FM_AD_DIE_CHIP;
			return fm_chip;
		}
	}

	for (i = 0; i < (sizeof(fm_support_chip_array)/sizeof(struct fm_chip_mapping)); i++) {
		if (chipid == fm_support_chip_array[i].con_chip) {
			fm_chip = fm_support_chip_array[i].fm_chip;
			break;
		}
	}

	if (type) {
		for (i = 0; i < (sizeof(fm_support_chip_array)/sizeof(struct fm_chip_mapping)); i++) {
			if (chipid == fm_support_chip_array[i].fm_chip) {
				*type = fm_support_chip_array[i].type;
				break;
			}
		}
	}

	return fm_chip;
}

signed int fm_open(struct fm *fmp)
{
	signed int ret = 0;
	signed int chipid = 0;

	if (fmp == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (fmp->chipon == false) {
		if (FM_LOCK(fm_ops_lock))
			return -FM_ELOCK;

		if (fm_wcn_ops.ei.wmt_chipid_query)
			chipid = fm_wcn_ops.ei.wmt_chipid_query();
		fmp->projectid = chipid;
		WCN_DBG(FM_NTC | MAIN, "wmt chip id=0x%x\n", chipid);

		if (fm_wcn_ops.ei.get_top_index)
			top_index = fm_wcn_ops.ei.get_top_index();
		else
			top_index = 4;
		WCN_DBG(FM_NTC | MAIN, "mcu top index = 0x%x\n", top_index);
		/* what's the purpose of put chipid to fmp->chip_id ? */
		fmp->chip_id = fm_which_chip(chipid, NULL);
		WCN_DBG(FM_NTC | MAIN, "fm chip id=0x%x\n", fmp->chip_id);

		fm_eint_pin_cfg(FM_EINT_PIN_EINT_MODE);
		fm_request_eint(fm_eint_handler);

		FM_UNLOCK(fm_ops_lock);
	}
	return ret;
}

signed int fm_close(struct fm *fmp)
{
	signed int ret = 0;

	if (fmp == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	fm_eint_pin_cfg(FM_EINT_PIN_GPIO_MODE);

	FM_UNLOCK(fm_ops_lock);

	return ret;
}

signed int fm_rds_read(struct fm *fmp, signed char *dst, signed int len)
{
	signed int copy_len = 0, left = 0;

	copy_len = sizeof(struct rds_t);

	if (FM_EVENT_GET(fmp->rds_event) == FM_RDS_DATA_READY) {
		if (FM_LOCK(fm_read_lock))
			return -FM_ELOCK;

		left = copy_to_user((void *)dst, fmp->pstRDSData, (unsigned long)copy_len);
		if (left)
			WCN_DBG(FM_ALT | MAIN, "fm_read copy failed\n");
		else
			fmp->pstRDSData->event_status = 0x0000;

		WCN_DBG(FM_DBG | MAIN, "fm_read copy len:%d\n", (copy_len - left));

		FM_EVENT_RESET(fmp->rds_event);
		FM_UNLOCK(fm_read_lock);
	} else {
		/*if (FM_EVENT_WAIT(fmp->rds_event, FM_RDS_DATA_READY) == 0) {
		 * WCN_DBG(FM_DBG | MAIN, "fm_read wait ok\n");
		 *  goto RESTART;
		 *  } else {
		 *  WCN_DBG(FM_ALT | MAIN, "fm_read wait err\n");
		 *  return 0;
		 *  } *//*event wait caused AP stop RDS thread and re-read RDS, which caused issue ALPS00595367 */

		WCN_DBG(FM_DBG | MAIN, "fm_read no event now\n");
		return 0;
	}

	return copy_len - left;
}

signed int fm_powerup(struct fm *fm, struct fm_tune_parm *parm)
{
	signed int ret = 0;
	unsigned char tmp_vol;

	if (fm_low_ops.bi.pwron == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (fm_low_ops.bi.pwrupseq == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	/* for normal case */
	if (fm_low_ops.bi.pwron == NULL) {
		WCN_DBG(FM_NTC | MAIN, "get fm_low_ops fail\n");
		ret = -ENODEV;
		goto out;
	}

	if (fm->chipon == false) {
		ret = fm_low_ops.bi.pwron(0);
		if (ret) {
			ret = -ENODEV;
			goto out;
		}
		fm->chipon = true;
	}

	if (fm_pwr_state_get(fm) == FM_PWR_RX_ON) {
		WCN_DBG(FM_NTC | MAIN, "already pwron!\n");
		goto out;
	} else if (fm_pwr_state_get(fm) == FM_PWR_TX_ON) {
		/* if Tx is on, we need pwr down TX first */
		WCN_DBG(FM_NTC | MAIN, "power down TX first!\n");

		ret = fm_powerdowntx(fm);
		if (ret) {
			WCN_DBG(FM_ERR | MAIN, "FM pwr down Tx fail!\n");
			return ret;
		}
	}

	fm_pwr_state_set(fm, FM_PWR_RX_ON);

	/* execute power on sequence */
	ret = fm_low_ops.bi.pwrupseq(&fm->chip_id, &fm->device_id);
	if (ret) {
		fm_pwr_state_set(fm, FM_PWR_OFF);
		WCN_DBG(FM_ERR | MAIN, "powerup fail!!!\n");
		goto out;
	}

	fm_cur_freq_set(parm->freq);

	parm->err = FM_SUCCESS;
	if (fm_low_ops.bi.low_pwr_wa)
		fm_low_ops.bi.low_pwr_wa(1);

	fm_low_ops.bi.volget(&tmp_vol);
	WCN_DBG(FM_INF | MAIN, "vol=%d!!!\n", tmp_vol);

	/* fm_low_ops.bi.volset(0); */
	fm->vol = 15;
	if (fm_low_ops.ri.rds_bci_get) {
		fm_timer_sys->init(fm_timer_sys, fm_timer_func, (unsigned long)g_fm_struct,
				   fm_low_ops.ri.rds_bci_get(), 0);
		fm_timer_sys->start(fm_timer_sys);
	} else {
		WCN_DBG(FM_NTC | MAIN, "start timer fail!!!\n");
	}

out:
	FM_UNLOCK(fm_ops_lock);
	return ret;
}

/*
 *  fm_powerup_tx
 */
signed int fm_powerup_tx(struct fm *fm, struct fm_tune_parm *parm)
{
	signed int ret = 0;

	if (fm_low_ops.bi.pwron == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (fm_low_ops.bi.pwrupseq_tx == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (fm_pwr_state_get(fm) == FM_PWR_TX_ON) {
		WCN_DBG(FM_NTC | MAIN, "already pwron!\n");
		parm->err = FM_BADSTATUS;
		goto out;
	} else if (fm_pwr_state_get(fm) == FM_PWR_RX_ON) {
		/* if Rx is on, we need pwr down  first */
		ret = fm_powerdown(fm, 0);
		if (ret) {
			WCN_DBG(FM_ERR | MAIN, "FM pwr down Rx fail!\n");
			goto out;
		}
	}

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	/* for normal case */
	if (fm->chipon == false) {
		fm_low_ops.bi.pwron(0);
		fm->chipon = true;
	}

	fm_pwr_state_set(fm, FM_PWR_TX_ON);
	ret = fm_low_ops.bi.pwrupseq_tx();

	if (ret) {
		parm->err = FM_FAILED;
		fm_pwr_state_set(fm, FM_PWR_OFF);
		WCN_DBG(FM_ERR | MAIN, "FM pwr up Tx fail!\n");
	} else {
		parm->err = FM_SUCCESS;
	}
	fm_cur_freq_set(parm->freq);

out:
	FM_UNLOCK(fm_ops_lock);
	return ret;
}

static signed int pwrdown_flow(struct fm *fm)
{
	signed int ret = 0;

	if (fm_low_ops.ri.rds_onoff == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (fm_low_ops.bi.pwrdownseq == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (fm_pwr_state_get(fm) == FM_PWR_OFF) {
		WCN_DBG(FM_NTC | MAIN, "FMSYS already pwroff!\n");
		goto out;
	}

	if (fm_pwr_state_get(fm) == FM_PWR_RX_ON) {
		/* Disable all interrupt */
		fm_disable_rds_BlerCheck();
		fm_low_ops.ri.rds_onoff(fm->pstRDSData, false);

		fm_pwr_state_set(fm, FM_PWR_OFF);

		/* execute power down sequence */
		ret = fm_low_ops.bi.pwrdownseq();

		if (fm_low_ops.bi.low_pwr_wa)
			fm_low_ops.bi.low_pwr_wa(0);

		WCN_DBG(FM_ALT | MAIN, "pwrdown_flow exit\n");
	}
out:
	return ret;
}

signed int fm_powerdown(struct fm *fm, int type)
{
	signed int ret = 0;

#if (FM_INVALID_CHAN_NOISE_REDUCING)
	fm_cqi_check_timer->stop(fm_cqi_check_timer);
#endif

	if (type == 1) {	/* 0: RX 1: TX */
		ret = fm_powerdowntx(fm);
	} else {
		if (FM_LOCK(fm_ops_lock))
			return -FM_ELOCK;
		if (FM_LOCK(fm_rxtx_lock))
			return -FM_ELOCK;

		ret = pwrdown_flow(fm);

		FM_UNLOCK(fm_rxtx_lock);
		FM_UNLOCK(fm_ops_lock);
	}

	if ((fm_pwr_state_get(fm) == FM_PWR_OFF) && (fm->chipon == true)) {
		fm_low_ops.bi.pwroff(0);
		fm->chipon = false;
	}

	return ret;
}

signed int fm_powerdowntx(struct fm *fm)
{
	signed int ret = 0;

	if (fm_low_ops.bi.pwrdownseq_tx == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_rxtx_lock))
		return -FM_ELOCK;

	if (fm_pwr_state_get(fm) == FM_PWR_TX_ON) {
		/* fm_low_ops.ri.rds_onoff(fm->pstRDSData, false); */
		/* execute power down sequence */
		ret = fm_low_ops.bi.pwrdownseq_tx();
		if (ret)
			WCN_DBG(FM_ERR | MAIN, "pwrdown tx fail\n");

		fm_pwr_state_set(fm, FM_PWR_OFF);
		WCN_DBG(FM_NTC | MAIN, "pwrdown tx ok\n");
	}

	FM_UNLOCK(fm_rxtx_lock);
	/* FM_UNLOCK(fm_ops_lock); */
	return ret;
}

/***********************************************************
* Function:	fm_tx_scan()
*
* Description:	get the valid channels for fm tx function
*
* Para:		fm--->fm driver global info
*			parm--->input/output parameter
*
* Return:		0, if success; error code, if failed
***********************************************************/
signed int fm_tx_scan(struct fm *fm, struct fm_tx_scan_parm *parm)
{
	signed int ret = 0;
	unsigned short scandir = 0;
	unsigned short space = parm->space;

	if (fm_low_ops.bi.tx_scan == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if (fm->chipon != true) {
		parm->err = FM_BADSTATUS;
		ret = -EPERM;
		WCN_DBG(FM_ERR | MAIN, "tx scan chip not on\n");
		goto out;
	}
	switch (parm->scandir) {
	case FM_TX_SCAN_UP:
		scandir = 0;
		break;
	case FM_TX_SCAN_DOWN:
		scandir = 1;
		break;
	default:
		scandir = 0;
		break;
	}

	if (parm->band == FM_BAND_UE) {
		fm->min_freq = FM_UE_FREQ_MIN;
		fm->max_freq = FM_UE_FREQ_MAX;
	} else if (parm->band == FM_BAND_JAPANW) {
		fm->min_freq = FM_JP_FREQ_MIN;
		fm->max_freq = FM_JP_FREQ_MAX;
	} else if (parm->band == FM_BAND_SPECIAL) {
		fm->min_freq = FM_FREQ_MIN;
		fm->max_freq = FM_FREQ_MAX;
	} else {
		WCN_DBG(FM_ERR | MAIN, "band:%d out of range\n", parm->band);
		parm->err = FM_EPARM;
		ret = -EPERM;
		goto out;
	}

	if (unlikely((parm->freq < fm->min_freq) || (parm->freq > fm->max_freq))) {
		parm->err = FM_EPARM;
		ret = -EPERM;
		WCN_DBG(FM_ERR | MAIN, "%s parm->freq:%d fm->min_freq: %d fm->max_freq:%d\n",
		__func__, parm->freq, fm->min_freq, fm->max_freq);
		goto out;
	}

	if (unlikely(parm->ScanTBLSize < TX_SCAN_MIN || parm->ScanTBLSize > TX_SCAN_MAX)) {
		parm->err = FM_EPARM;
		ret = -EPERM;
		WCN_DBG(FM_ERR | MAIN, "%s parm->ScanTBLSize:%d TX_SCAN_MIN:%d TX_SCAN_MAX:%d\n",
		__func__, parm->ScanTBLSize, TX_SCAN_MIN, TX_SCAN_MAX);
		goto out;
	}

	ret = fm_low_ops.bi.anaswitch(FM_ANA_SHORT);
	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "switch to short ana failed\n");
		goto out;
	}
	/* do tx scan */
	ret = fm_low_ops.bi.tx_scan(fm->min_freq, fm->max_freq, &(parm->freq),
					  parm->ScanTBL, &(parm->ScanTBLSize), scandir, space);
	if (!ret) {
		parm->err = FM_SUCCESS;
	} else {
		WCN_DBG(FM_ERR | MAIN, "fm_tx_scan failed\n");
		parm->err = FM_SCAN_FAILED;
	}
out:
	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_cqi_get(struct fm *fm, signed int ch_num, signed char *buf, signed int buf_size)
{
	signed int ret = 0;
	signed int idx = 0;

	if (fm_low_ops.bi.cqi_get == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if (true == scan_stop_flag) {
		WCN_DBG(FM_NTC | MAIN, "scan flow aborted, do not get CQI\n");
		ret = -1;
		goto out;
	}

	if (fm_pwr_state_get(fm) != FM_PWR_RX_ON) {
		ret = -EPERM;
		goto out;
	}

	if (ch_num * sizeof(struct fm_cqi) > buf_size) {
		ret = -EPERM;
		goto out;
	}

	fm_op_state_set(fm, FM_STA_SCAN);

	idx = 0;
	WCN_DBG(FM_NTC | MAIN, "cqi num %d\n", ch_num);

	while (ch_num > 0) {
		ret =
		    fm_low_ops.bi.cqi_get(buf + 16 * sizeof(struct fm_cqi) * idx,
					  buf_size - 16 * sizeof(struct fm_cqi) * idx);

		if (ret)
			goto out;

		ch_num -= 16;
		idx++;
	}

	fm_op_state_set(fm, FM_STA_STOP);

out:
	FM_UNLOCK(fm_ops_lock);
	return ret;
}

/*  fm_is_dese_chan -- check if gived channel is a de-sense channel or not
  *  @pfm - fm driver global DS
  *  @freq - gived channel
  *  return value: 0, not a dese chan; 1, a dese chan; else error NO.
  */
signed int fm_is_dese_chan(struct fm *pfm, unsigned short freq)
{
	signed int ret = 0;

	if (pfm == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (fm_low_ops.bi.is_dese_chan) {
		if (FM_LOCK(fm_ops_lock))
			return -FM_ELOCK;
		ret = fm_low_ops.bi.is_dese_chan(freq);
		FM_UNLOCK(fm_ops_lock);
	}

	return ret;
}

/*  fm_is_dese_chan -- check if gived channel is a de-sense channel or not
  *  @pfm - fm driver global DS
  *  @freq - gived channel
  *  return value: 0, not a dese chan; 1, a dese chan; else error NO.
  */
signed int fm_desense_check(struct fm *pfm, unsigned short freq, signed int rssi)
{
	signed int ret = 0;

	if (pfm == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (fm_low_ops.bi.desense_check) {
		if (FM_LOCK(fm_ops_lock))
			return -FM_ELOCK;
		ret = fm_low_ops.bi.desense_check(freq, rssi);
		FM_UNLOCK(fm_ops_lock);
	}

	return ret;
}

signed int fm_dump_reg(void)
{
	signed int ret = 0;

	if (fm_low_ops.bi.dumpreg) {
		if (FM_LOCK(fm_ops_lock))
			return -FM_ELOCK;
		ret = fm_low_ops.bi.dumpreg();
		FM_UNLOCK(fm_ops_lock);
	}
	return ret;
}

/*  fm_get_hw_info -- hw info: chip id, ECO version, DSP ROM version, Patch version
  *  @pfm - fm driver global DS
  *  @freq - target buffer
  *  return value: 0, success; else error NO.
  */
signed int fm_get_hw_info(struct fm *pfm, struct fm_hw_info *req)
{
	signed int ret = 0;

	if (req == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* default value for all chips */
	req->chip_id = 0x000066FF;
	req->eco_ver = 0x00000000;
	req->rom_ver = 0x00000001;
	req->patch_ver = 0x00000100;
	req->reserve = 0x00000000;

	/* get actual chip hw info */
	if (fm_low_ops.bi.hwinfo_get) {
		if (FM_LOCK(fm_ops_lock))
			return -FM_ELOCK;
		ret = fm_low_ops.bi.hwinfo_get(req);
		FM_UNLOCK(fm_ops_lock);
	}

	return ret;
}

signed int fm_get_aud_info(struct fm_audio_info_t *data)
{
	if (fm_low_ops.bi.get_aud_info)
		return fm_low_ops.bi.get_aud_info(data);

	data->aud_path = FM_AUD_ERR;
	data->i2s_info.mode = FM_I2S_MODE_ERR;
	data->i2s_info.status = FM_I2S_STATE_ERR;
	data->i2s_info.rate = FM_I2S_SR_ERR;
	data->i2s_pad = FM_I2S_PAD_ERR;
	return 0;
}

/*  fm_get_i2s_info -- i2s info: on/off, master/slave, sample rate
  *  @pfm - fm driver global DS
  *  @req - target buffer
  *  return value: 0, success; else error NO.
  */
signed int fm_get_i2s_info(struct fm *pfm, struct fm_i2s_info *req)
{
	if (fm_low_ops.bi.i2s_get == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	return fm_low_ops.bi.i2s_get(&req->status, &req->mode, &req->rate);
}

signed int fm_hwscan_stop(struct fm *fm)
{
	signed int ret = 0;

	if ((fm_op_state_get(fm) != FM_STA_SCAN) && (fm_op_state_get(fm) != FM_STA_SEEK)) {
		WCN_DBG(FM_WAR | MAIN, "fm isn't on scan, no need stop\n");
		return ret;
	}

	if (fm_low_ops.bi.scanstop == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	fm_low_ops.bi.scanstop();
	fm_low_ops.bi.seekstop();
	scan_stop_flag = true;
	WCN_DBG(FM_DBG | MAIN, "fm will stop scan\n");

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	fm_low_ops.bi.rampdown();
	fm_low_ops.bi.setfreq(fm_cur_freq_get());

	FM_UNLOCK(fm_ops_lock);

	return ret;
}

/* fm_ana_switch -- switch antenna to long/short
 * @fm - fm driver main data structure
 * @antenna - 0, long; 1, short
 * If success, return 0; else error code
 */
signed int fm_ana_switch(struct fm *fm, signed int antenna)
{
	signed int ret = 0;

	if (fm_low_ops.bi.anaswitch == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	WCN_DBG(FM_DBG | MAIN, "Switching ana to %s\n", antenna ? "short" : "long");
	fm->ana_type = antenna;

	if ((fm_pwr_state_get(fm) == FM_PWR_RX_ON) || (fm_pwr_state_get(fm) == FM_PWR_TX_ON))
		ret = fm_low_ops.bi.anaswitch(antenna);

	if (ret)
		WCN_DBG(FM_ALT | MAIN, "Switch ana Failed\n");
	else
		WCN_DBG(FM_DBG | MAIN, "Switch ana OK!\n");

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

/* volume?[0~15] */
signed int fm_setvol(struct fm *fm, unsigned int vol)
{
	unsigned char tmp_vol;

	if (fm_pwr_state_get(fm) != FM_PWR_RX_ON)
		return -EPERM;

	if (fm_low_ops.bi.volset == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	tmp_vol = (vol > 15) ? 15 : vol;
	fm_low_ops.bi.volset(tmp_vol);
	fm->vol = (signed int) tmp_vol;

	FM_UNLOCK(fm_ops_lock);
	return 0;
}

signed int fm_getvol(struct fm *fm, unsigned int *vol)
{
	unsigned char tmp_vol;

	if (fm_pwr_state_get(fm) != FM_PWR_RX_ON)
		return -EPERM;

	if (fm_low_ops.bi.volget == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	fm_low_ops.bi.volget(&tmp_vol);
	*vol = (unsigned int) tmp_vol;

	FM_UNLOCK(fm_ops_lock);
	return 0;
}

signed int fm_mute(struct fm *fm, unsigned int bmute)
{
	signed int ret = 0;

	if (fm_pwr_state_get(fm) != FM_PWR_RX_ON) {
		ret = -EPERM;
		return ret;
	}
	if (fm_low_ops.bi.mute == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if (bmute) {
		ret = fm_low_ops.bi.mute(true);
		fm->mute = true;
	} else {
		ret = fm_low_ops.bi.mute(false);
		fm->mute = false;
	}

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_getrssi(struct fm *fm, signed int *rssi)
{
	signed int ret = 0;

	if (fm_pwr_state_get(fm) != FM_PWR_RX_ON) {
		ret = -EPERM;
		return ret;
	}
	if (fm_low_ops.bi.rssiget == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_low_ops.bi.rssiget(rssi);

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_read(struct fm *fm, unsigned char addr, unsigned short *val)
{
	signed int ret = 0;

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_reg_read(addr, val);

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_write(struct fm *fm, unsigned char addr, unsigned short val)
{
	signed int ret = 0;

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_reg_write(addr, val);

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_top_read(struct fm *fm, unsigned short addr, unsigned int *val)
{
	signed int ret = 0;

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_top_reg_read(addr, val);

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_top_write(struct fm *fm, unsigned short addr, unsigned int val)
{
	signed int ret = 0;

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_top_reg_write(addr, val);

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_host_read(struct fm *fm, unsigned int addr, unsigned int *val)
{
	signed int ret = 0;

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_host_reg_read(addr, val);

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_host_write(struct fm *fm, unsigned int addr, unsigned int val)
{
	signed int ret = 0;

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_host_reg_write(addr, val);

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_pmic_read(struct fm *fm, unsigned char addr, unsigned int *val)
{
	signed int ret = 0;

	if (fm_low_ops.bi.pmic_read == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_low_ops.bi.pmic_read(addr, val);

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_pmic_write(struct fm *fm, unsigned char addr, unsigned int val)
{
	signed int ret = 0;

	if (fm_low_ops.bi.pmic_write == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_low_ops.bi.pmic_write(addr, val);

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_chipid_get(struct fm *fm, unsigned short *chipid)
{
	if (chipid == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	*chipid = fm->chip_id;

	FM_UNLOCK(fm_ops_lock);
	return 0;
}

signed int fm_monostereo_get(struct fm *fm, unsigned short *ms)
{
	signed int ret = 0;

	if (fm_low_ops.bi.msget == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (ms == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if (fm_low_ops.bi.msget(ms) == false)
		ret = -FM_EPARA;

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

/*
 * Force set to stero/mono mode
 * @MonoStereo -- 0, auto; 1, mono
 * If success, return 0; else error code
 */
signed int fm_monostereo_set(struct fm *fm, signed int ms)
{
	signed int ret = 0;

	if (fm_low_ops.bi.msset == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_low_ops.bi.msset(ms);

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_pamd_get(struct fm *fm, unsigned short *pamd)
{
	signed int ret = 0;

	if (fm_low_ops.bi.pamdget == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (pamd == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if (fm_low_ops.bi.pamdget(pamd) == false)
		ret = -FM_EPARA;

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_caparray_get(struct fm *fm, signed int *ca)
{
	signed int ret = 0;

	if (fm_low_ops.bi.caparray_get == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (ca == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_low_ops.bi.caparray_get(ca);

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_em_test(struct fm *fm, unsigned short group, unsigned short item, unsigned int val)
{
	signed int ret = 0;

	if (fm_low_ops.bi.em == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if (false == fm_low_ops.bi.em(group, item, val))
		ret = -FM_EPARA;

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_set_search_th(struct fm *fm, struct fm_search_threshold_t parm)
{
	signed int ret = 0;

	if (fm_low_ops.bi.set_search_th == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;
	ret = fm_low_ops.bi.set_search_th(parm.th_type, parm.th_val, parm.reserve);

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_rds_tx(struct fm *fm, struct fm_rds_tx_parm *parm)
{
	signed int ret = 0;

	if (fm_low_ops.ri.rds_tx == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (fm_pwr_state_get(fm) != FM_PWR_TX_ON) {
		parm->err = FM_BADSTATUS;
		ret = -FM_EPARA;
		goto out;
	}
	if (parm->other_rds_cnt > 29) {
		parm->err = FM_EPARM;
		WCN_DBG(FM_ERR | MAIN, "other_rds_cnt=%d\n", parm->other_rds_cnt);
		ret = -FM_EPARA;
		goto out;
	}

	ret = fm_low_ops.ri.rds_tx(parm->pi, parm->ps, parm->other_rds, parm->other_rds_cnt);
	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "Rds_Tx failed!\n");
		goto out;
	}
/* fm_cxt->txcxt.rdsTxOn = true; */
/* fm_cxt->txcxt.pi = parm->pi; */
/* memcpy(fm_cxt->txcxt.ps, parm->ps,sizeof(parm->ps)); */
/* memcpy(fm_cxt->txcxt.other_rds, parm->other_rds,sizeof(parm->other_rds)); */
/* fm_cxt->txcxt.other_rds_cnt = parm->other_rds_cnt; */
out:
	return ret;
}

signed int fm_rds_onoff(struct fm *fm, unsigned short rdson_off)
{
	signed int ret = 0;

	if (fm_pwr_state_get(fm) != FM_PWR_RX_ON) {
		ret = -EPERM;
		goto out;
	}
	if (fm_low_ops.ri.rds_onoff == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if (rdson_off) {
		fm->rds_on = true;
		if (fm_low_ops.ri.rds_onoff(fm->pstRDSData, true) == false) {
			WCN_DBG(FM_ALT | MAIN, "FM_IOCTL_RDS_ONOFF on faield\n");
			ret = -EPERM;
			goto out;
		}

		fm_enable_rds_BlerCheck(fm);
	} else {
		fm->rds_on = false;
		fm_disable_rds_BlerCheck();
		if (fm_low_ops.ri.rds_onoff(fm->pstRDSData, false) == false) {
			WCN_DBG(FM_ALT | MAIN, "FM_IOCTL_RDS_ONOFF off faield\n");
			ret = -EPERM;
		};
	}

out:
	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_rds_good_bc_get(struct fm *fm, unsigned short *gbc)
{
	signed int ret = 0;

	if (fm_low_ops.ri.rds_gbc_get == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (gbc == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	*gbc = fm_low_ops.ri.rds_gbc_get();

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_rds_bad_bc_get(struct fm *fm, unsigned short *bbc)
{
	signed int ret = 0;

	if (fm_low_ops.ri.rds_gbc_get == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (bbc == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	*bbc = fm_low_ops.ri.rds_bbc_get();

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_rds_bler_ratio_get(struct fm *fm, unsigned short *bbr)
{
	signed int ret = 0;

	if (fm_low_ops.ri.rds_bbr_get == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (bbr == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	*bbr = (unsigned short) fm_low_ops.ri.rds_bbr_get();

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_rds_group_cnt_get(struct fm *fm, struct rds_group_cnt_t *dst)
{
	signed int ret = 0;

	if (fm_low_ops.ri.rds_gc_get == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dst == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_rds_cnt))
		return -FM_ELOCK;

	ret = fm_low_ops.ri.rds_gc_get(dst, fm->pstRDSData);

	FM_UNLOCK(fm_rds_cnt);
	return ret;
}

signed int fm_rds_group_cnt_reset(struct fm *fm)
{
	signed int ret = 0;

	if (fm_low_ops.ri.rds_gc_reset == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_rds_cnt))
		return -FM_ELOCK;

	ret = fm_low_ops.ri.rds_gc_reset(fm->pstRDSData);

	FM_UNLOCK(fm_rds_cnt);
	return ret;
}

signed int fm_rds_log_get(struct fm *fm, struct rds_rx_t *dst, signed int *dst_len)
{
	signed int ret = 0;

	if (fm_low_ops.ri.rds_log_get == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dst == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dst_len == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_read_lock))
		return -FM_ELOCK;

	ret = fm_low_ops.ri.rds_log_get(dst, dst_len);

	FM_UNLOCK(fm_read_lock);
	return ret;
}

signed int fm_rds_block_cnt_reset(struct fm *fm)
{
	signed int ret = 0;

	if (fm_low_ops.ri.rds_bc_reset == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_low_ops.ri.rds_bc_reset();

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_i2s_set(struct fm *fm, signed int onoff, signed int mode, signed int sample)
{
	signed int ret = 0;

	if (fm_low_ops.bi.i2s_set == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if ((onoff != 0) && (onoff != 1))
		onoff = 0; /* default set i2s on in case user input illegal.*/

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if ((fm_pwr_state_get(fm) == FM_PWR_RX_ON) || (fm_pwr_state_get(fm) == FM_PWR_TX_ON))
		ret = fm_low_ops.bi.i2s_set(onoff, mode, sample);
	if (ret)
		WCN_DBG(FM_ALT | MAIN, "i2s setting Failed\n");
	else
		WCN_DBG(FM_INF | MAIN, "i2s setting OK!\n");

	FM_UNLOCK(fm_ops_lock);
	return ret;
}

/*
 *  fm_tune_tx
 */
signed int fm_tune_tx(struct fm *fm, struct fm_tune_parm *parm)
{
	signed int ret = 0;

	if (fm_low_ops.bi.tune_tx == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (fm_pwr_state_get(fm) != FM_PWR_TX_ON) {
		parm->err = FM_BADSTATUS;
		return -EPERM;
	}
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	WCN_DBG(FM_DBG | MAIN, "%s\n", __func__);

	fm_op_state_set(fm, FM_STA_TUNE);
	WCN_DBG(FM_NTC | MAIN, "Tx tune to %d\n", parm->freq);
	/* tune to desired channel */
	if (true != fm_low_ops.bi.tune_tx(parm->freq)) {
		parm->err = FM_TUNE_FAILED;
		WCN_DBG(FM_ALT | MAIN, "Tx tune failed\n");
		ret = -EPERM;
	}
	fm_op_state_set(fm, FM_STA_PLAY);
	FM_UNLOCK(fm_ops_lock);

	return ret;
}

/*
 *  fm_tune
 */
signed int fm_tune(struct fm *fm, struct fm_tune_parm *parm)
{
	signed int ret = 0;
	signed int len = 0;
	struct rds_raw_t rds_log;

#if (FM_INVALID_CHAN_NOISE_REDUCING)
	fm_cqi_check_timer->stop(fm_cqi_check_timer);
#endif

	if (fm_low_ops.bi.mute == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (fm_low_ops.bi.rampdown == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (fm_low_ops.bi.setfreq == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	WCN_DBG(FM_INF | MAIN, "%s\n", __func__);

	/* clean RDS first in case RDS event report before tune success */
	/* clean RDS data */
	fm_memset(fm->pstRDSData, 0, sizeof(struct rds_t));

	/* clean RDS log buffer */
	do {
		ret = fm_rds_log_get(fm, (struct rds_rx_t *)&(rds_log.data), &len);
		rds_log.dirty = true;
		rds_log.len = (len < sizeof(rds_log.data)) ? len : sizeof(rds_log.data);
		WCN_DBG(FM_ALT | MAIN, "clean rds log, rds_log.len =%d\n", rds_log.len);
		if (ret < 0)
			break;
	} while (rds_log.len > 0);

	if (fm_pwr_state_get(fm) != FM_PWR_RX_ON) {
		parm->err = FM_BADSTATUS;
		ret = -EPERM;
		goto out;
	}
/* fm_low_ops.bi.mute(true); */
	ret = fm_low_ops.bi.rampdown();
	if (ret) {
		WCN_DBG(FM_ALT | MAIN, "FM ramp down failed\n");
		goto out;
	}

	fm_op_state_set(fm, FM_STA_TUNE);
	WCN_DBG(FM_ALT | MAIN, "tuning to %d\n", parm->freq);

	if (true != fm_low_ops.bi.setfreq(parm->freq)) {
		parm->err = FM_TUNE_FAILED;
		WCN_DBG(FM_ALT | MAIN, "FM tune failed\n");
		ret = -FM_EFW;
	}

#if (FM_INVALID_CHAN_NOISE_REDUCING)
	if (fm_low_ops.bi.is_valid_freq) {
		if (fm_low_ops.bi.is_valid_freq(parm->freq)) {
			if (fm->vol != 15) {
				fm_low_ops.bi.volset(15);
				fm->vol = 15;
				WCN_DBG(FM_NTC | MAIN, "Valid freq, volset: 15\n");
			}
			WCN_DBG(FM_NTC | MAIN, "FM tune to a valid channel resume volume.\n");
		} else {
			if (fm->vol != 5) {
				fm_low_ops.bi.volset(5);
				fm->vol = 5;
				WCN_DBG(FM_NTC | MAIN, "Not a valid freq, volset: 5\n");
			}
			WCN_DBG(FM_NTC | MAIN, "FM tune to an invalid channel.\n");
			fm_cqi_check_timer->start(fm_cqi_check_timer);
		}
	}
#endif
	/* fm_low_ops.bi.mute(false);//open for dbg */
	fm_op_state_set(fm, FM_STA_PLAY);
out:
	FM_UNLOCK(fm_ops_lock);
	return ret;
}

/* cqi log tool entry */
signed int fm_cqi_log(void)
{
	signed int ret = 0;
	unsigned short freq;

	if (fm_low_ops.bi.cqi_log == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	freq = fm_cur_freq_get();
	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	if ((freq != 10000) && (g_dbg_level != 0xffffffff))
		return -FM_EPARA;

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_low_ops.bi.cqi_log(8750, 10800, 2, 5);
	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_pre_search(struct fm *fm)
{
	signed int ret = 0;

	if (fm_low_ops.bi.pre_search == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (fm_pwr_state_get(fm) != FM_PWR_RX_ON)
		return -FM_EPARA;

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	WCN_DBG(FM_INF | MAIN, "%s\n", __func__);

	ret = fm_low_ops.bi.pre_search();
	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_restore_search(struct fm *fm)
{
	signed int ret = 0;

	if (fm_low_ops.bi.restore_search == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (fm_pwr_state_get(fm) != FM_PWR_RX_ON)
		return -FM_EPARA;

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	WCN_DBG(FM_INF | MAIN, "%s\n", __func__);

	ret = fm_low_ops.bi.restore_search();
	FM_UNLOCK(fm_ops_lock);
	return ret;
}

/*fm soft mute tune function*/
signed int fm_soft_mute_tune(struct fm *fm, struct fm_softmute_tune_t *parm)
{
	signed int ret = 0;

#if (FM_INVALID_CHAN_NOISE_REDUCING)
	fm_cqi_check_timer->stop(fm_cqi_check_timer);
#endif

	if (fm_low_ops.bi.softmute_tune == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if (fm_pwr_state_get(fm) != FM_PWR_RX_ON) {
		parm->valid = false;
		ret = -EPERM;
		goto out;
	}
	/* fm_low_ops.bi.mute(true); */
	/* fm_op_state_set(fm, FM_STA_TUNE); */

	if (false == fm_low_ops.bi.softmute_tune(parm->freq, &parm->rssi, &parm->valid)) {
		parm->valid = false;
		WCN_DBG(FM_ALT | MAIN, "sm tune failed\n");
		ret = -EPERM;
	}
/* fm_low_ops.bi.mute(false); */
	WCN_DBG(FM_INF | MAIN, "%s, freq=%d, rssi=%d, valid=%d\n", __func__, parm->freq, parm->rssi, parm->valid);

out:
	FM_UNLOCK(fm_ops_lock);

	return ret;
}

signed int fm_over_bt(struct fm *fm, signed int flag)
{
	signed int ret = 0;

	if (fm_low_ops.bi.fm_via_bt == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (fm_pwr_state_get(fm) != FM_PWR_RX_ON)
		return -EPERM;

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	ret = fm_low_ops.bi.fm_via_bt(flag);
	if (ret)
		WCN_DBG(FM_ALT | MAIN, "%s(),failed!\n", __func__);
	else
		fm->via_bt = flag;

	WCN_DBG(FM_NTC | MAIN, "%s(),[ret=%d]!\n", __func__, ret);
	FM_UNLOCK(fm_ops_lock);
	return ret;
}

signed int fm_tx_support(struct fm *fm, signed int *support)
{
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if (fm_low_ops.bi.tx_support)
		fm_low_ops.bi.tx_support(support);
	else
		*support = 0;

	WCN_DBG(FM_NTC | MAIN, "%s(),[%d]!\n", __func__, *support);
	FM_UNLOCK(fm_ops_lock);
	return 0;
}

signed int fm_rdstx_support(struct fm *fm, signed int *support)
{
	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;

	if (fm_low_ops.ri.rdstx_support)
		fm_low_ops.ri.rdstx_support(support);
	else
		*support = 0;

	WCN_DBG(FM_NTC | MAIN, "support=[%d]!\n", *support);
	FM_UNLOCK(fm_ops_lock);
	return 0;
}

/*1:on,0:off*/
signed int fm_rdstx_enable(struct fm *fm, signed int enable)
{
	signed int ret = -1;

	if (fm_low_ops.ri.rds_tx_enable == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (fm_low_ops.ri.rds_tx_disable == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (fm_pwr_state_get(fm) != FM_PWR_TX_ON)
		return -FM_EPARA;

	if (FM_LOCK(fm_ops_lock))
		return -FM_ELOCK;
	if (enable == 1) {
		ret = fm_low_ops.ri.rds_tx_enable();
		if (ret)
			WCN_DBG(FM_ERR | MAIN, "rds_tx_enable fail=[%d]!\n", ret);

		fm->rdstx_on = true;
	} else {
		ret = fm_low_ops.ri.rds_tx_disable();
		if (ret)
			WCN_DBG(FM_ERR | MAIN, "rds_tx_disable fail=[%d]!\n", ret);

		fm->rdstx_on = false;
	}
	WCN_DBG(FM_NTC | MAIN, "rds tx enable=[%d]!\n", enable);
	FM_UNLOCK(fm_ops_lock);
	return 0;
}

#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
static void fm_timer_func(struct timer_list *timer)
#else
static void fm_timer_func(unsigned long data)
#endif
{
	struct fm *fm = g_fm_struct;

	if (FM_LOCK(fm_timer_lock))
		return;

	if (fm_timer_sys->update(fm_timer_sys)) {
		WCN_DBG(FM_NTC | MAIN, "timer skip\n");
		goto out;	/* fm timer is stopped before timeout */
	}

	if (fm != NULL) {
		WCN_DBG(FM_DBG | MAIN, "timer:rds_wk\n");
		fm->timer_wkthd->add_work(fm->timer_wkthd, fm->rds_wk);
	}

out:
	FM_UNLOCK(fm_timer_lock);
}

#if (FM_INVALID_CHAN_NOISE_REDUCING)
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
static void fm_cqi_check_timer_func(struct timer_list *timer)
#else
static void fm_cqi_check_timer_func(unsigned long data)
#endif
{
	struct fm *fm = g_fm_struct;

	if (FM_LOCK(fm_timer_lock))
		return;

	if (fm_cqi_check_timer->update(fm_cqi_check_timer)) {
		WCN_DBG(FM_NTC | MAIN, "timer skip\n");
		goto out;	/* fm timer is stopped before timeout */
	}

	if (fm != NULL) {
		WCN_DBG(FM_DBG | MAIN, "timer:ch_valid_check_wk\n");
		fm->timer_wkthd->add_work(fm->timer_wkthd, fm->ch_valid_check_wk);
	}

out:
	FM_UNLOCK(fm_timer_lock);
}
#endif

static void fm_tx_power_ctrl_worker_func(struct work_struct *work)
{
	signed int ctrl = 0, ret = 0;
	struct fm *fm = g_fm_struct;

	WCN_DBG(FM_NTC | MAIN, "+%s():\n", __func__);

	if (fm_low_ops.bi.tx_pwr_ctrl == NULL)
		return;
	if (FM_LOCK(fm_rxtx_lock))
		return;

	if (fm_pwr_state_get(fm) != FM_PWR_TX_ON) {
		WCN_DBG(FM_ERR | MAIN, "FM is not on TX mode\n");
		goto out;
	}

	ctrl = fm->tx_pwr;
	WCN_DBG(FM_NTC | MAIN, "tx pwr %ddb\n", ctrl);
	ret = fm_low_ops.bi.tx_pwr_ctrl(fm_cur_freq_get(), &ctrl);
	if (ret)
		WCN_DBG(FM_ERR | MAIN, "tx_pwr_ctrl fail\n");

out:
	FM_UNLOCK(fm_rxtx_lock);
	WCN_DBG(FM_NTC | MAIN, "-%s()\n", __func__);
}

static void fm_tx_rtc_ctrl_worker_func(struct work_struct *work)
{
	signed int ret = 0;
	signed int ctrl = 0;
	struct fm_gps_rtc_info rtcInfo;
	/* struct timeval curTime; */
	/* struct fm *fm = (struct fm*)fm_cb; */
	unsigned long curTime = 0;

	WCN_DBG(FM_NTC | MAIN, "+%s():\n", __func__);

	if (FM_LOCK(fm_rtc_mutex))
		return;

	if (gps_rtc_info.flag == FM_GPS_RTC_INFO_NEW) {
		memcpy(&rtcInfo, &gps_rtc_info, sizeof(struct fm_gps_rtc_info));
		gps_rtc_info.flag = FM_GPS_RTC_INFO_OLD;
		FM_UNLOCK(fm_rtc_mutex);
	} else {
		WCN_DBG(FM_NTC | MAIN, "there's no new rtc drift info\n");
		FM_UNLOCK(fm_rtc_mutex);
		goto out;
	}

	if (rtcInfo.age > rtcInfo.ageThd) {
		WCN_DBG(FM_WAR | MAIN, "age over it's threshlod\n");
		goto out;
	}
	if ((rtcInfo.drift <= rtcInfo.driftThd) && (rtcInfo.drift >= -rtcInfo.driftThd)) {
		WCN_DBG(FM_WAR | MAIN, "drift over it's MIN threshlod\n");
		goto out;
	}

	if (rtcInfo.drift > FM_GPS_RTC_DRIFT_MAX) {
		WCN_DBG(FM_WAR | MAIN, "drift over it's +MAX threshlod\n");
		rtcInfo.drift = FM_GPS_RTC_DRIFT_MAX;
		goto out;
	} else if (rtcInfo.drift < -FM_GPS_RTC_DRIFT_MAX) {
		WCN_DBG(FM_WAR | MAIN, "drift over it's -MAX threshlod\n");
		rtcInfo.drift = -FM_GPS_RTC_DRIFT_MAX;
		goto out;
	}

	curTime = jiffies;
	if (((long)curTime - (long)rtcInfo.stamp) / HZ > rtcInfo.tvThd.tv_sec) {
		WCN_DBG(FM_WAR | MAIN, "time diff over it's threshlod\n");
		goto out;
	}
	if (fm_low_ops.bi.rtc_drift_ctrl != NULL) {
		ctrl = rtcInfo.drift;
		WCN_DBG(FM_NTC | MAIN, "RTC_drift_ctrl[0x%08x]\n", ctrl);

		ret = fm_low_ops.bi.rtc_drift_ctrl(fm_cur_freq_get(), &ctrl);
		if (ret)
			goto out;
	}
out:
	WCN_DBG(FM_NTC | MAIN, "-%s()\n", __func__);
}

static void fm_tx_desense_wifi_worker_func(struct work_struct *work)
{
	signed int ret = 0;
	signed int ctrl = 0;
	struct fm *fm = g_fm_struct;

	WCN_DBG(FM_NTC | MAIN, "+%s():\n", __func__);

	if (FM_LOCK(fm_rxtx_lock))
		return;

	if (fm_pwr_state_get(fm) != FM_PWR_TX_ON) {
		WCN_DBG(FM_ERR | MAIN, "FM is not on TX mode\n");
		goto out;
	}

	fm_tx_rtc_ctrl_worker_func(0);

	ctrl = fm->vcoon;
	if (fm_low_ops.bi.tx_desense_wifi) {
		WCN_DBG(FM_NTC | MAIN, "tx_desense_wifi[%d]\n", ctrl);
		ret = fm_low_ops.bi.tx_desense_wifi(fm_cur_freq_get(), &ctrl);
		if (ret)
			WCN_DBG(FM_ERR | MAIN, "tx_desense_wifi fail\n");
	}
out:
	FM_UNLOCK(fm_rxtx_lock);
	WCN_DBG(FM_NTC | MAIN, "-%s()\n", __func__);
}

/*
************************************************************************************
Function:	    fm_get_gps_rtc_info()

Description:	get GPS RTC drift info, and this function should not block

Date:		    2011/04/10

Return Value:   success:0, failed: error coe
************************************************************************************
*/
signed int fm_get_gps_rtc_info(struct fm_gps_rtc_info *src)
{
	signed int ret = 0;
/* signed int retry_cnt = 0; */
	struct fm_gps_rtc_info *dst = &gps_rtc_info;

	if (src == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dst == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (src->retryCnt > 0) {
		dst->retryCnt = src->retryCnt;
		WCN_DBG(FM_NTC | MAIN, "%s, new [retryCnt=%d]\n", __func__, dst->retryCnt);
	}
	if (src->ageThd > 0) {
		dst->ageThd = src->ageThd;
		WCN_DBG(FM_NTC | MAIN, "%s, new [ageThd=%d]\n", __func__, dst->ageThd);
	}
	if (src->driftThd > 0) {
		dst->driftThd = src->driftThd;
		WCN_DBG(FM_NTC | MAIN, "%s, new [driftThd=%d]\n", __func__, dst->driftThd);
	}
	if (src->tvThd.tv_sec > 0) {
		dst->tvThd.tv_sec = src->tvThd.tv_sec;
		WCN_DBG(FM_NTC | MAIN, "%s, new [tvThd=%d]\n", __func__, (signed int) dst->tvThd.tv_sec);
	}
	ret = fm_rtc_mutex->trylock(fm_rtc_mutex, dst->retryCnt);
	if (ret)
		goto out;

	dst->age = src->age;
	dst->drift = src->drift;
	dst->stamp = jiffies;	/* get curren time stamp */
	dst->flag = FM_GPS_RTC_INFO_NEW;

	FM_UNLOCK(fm_rtc_mutex);


out:
	return ret;
}

static void fm_enable_rds_BlerCheck(struct fm *fm)
{
	if (FM_LOCK(fm_timer_lock))
		return;
	fm_timer_sys->start(fm_timer_sys);
	FM_UNLOCK(fm_timer_lock);
	WCN_DBG(FM_INF | MAIN, "enable rds timer ok\n");
}

static void fm_disable_rds_BlerCheck(void)
{
	if (FM_LOCK(fm_timer_lock))
		return;
	fm_timer_sys->stop(fm_timer_sys);
	FM_UNLOCK(fm_timer_lock);
	WCN_DBG(FM_INF | MAIN, "stop rds timer ok\n");
}

void fm_rds_reset_work_func(struct work_struct *work)
{
	signed int ret = 0;

	if (!fm_low_ops.ri.rds_blercheck)
		return;

	if (FM_LOCK(fm_rxtx_lock))
		return;

	if (FM_LOCK(fm_rds_cnt))
		return;

	ret = fm_low_ops.ri.rds_blercheck(g_fm_struct->pstRDSData);

	if (ret || g_fm_struct->pstRDSData->AF_Data.Addr_Cnt || g_fm_struct->pstRDSData->event_status) {
		WCN_DBG(FM_NTC | MAIN, "ret=%d, Addr_Cnt=0x%02x, event_status=0x%04x\n", ret,
			g_fm_struct->pstRDSData->AF_Data.Addr_Cnt, g_fm_struct->pstRDSData->event_status);
	}
	/* check af list get,can't use event==af_list because event will clear after read rds every time */
	if (g_fm_struct->pstRDSData->AF_Data.Addr_Cnt == 0xFF)
		g_fm_struct->pstRDSData->event_status |= RDS_EVENT_AF;

	if (!ret && g_fm_struct->pstRDSData->event_status)
		FM_EVENT_SEND(g_fm_struct->rds_event, FM_RDS_DATA_READY);

	FM_UNLOCK(fm_rds_cnt);
	FM_UNLOCK(fm_rxtx_lock);
}

#if (FM_INVALID_CHAN_NOISE_REDUCING)
void fm_cqi_check_work_func(struct work_struct *work)
{
	struct fm *fm = g_fm_struct;

	if (!fm)
		return;

	if (FM_LOCK(fm_ops_lock))
		return;

	if (fm_low_ops.bi.is_valid_freq) {
		if (fm_low_ops.bi.is_valid_freq(fm->cur_freq)) {
			if (fm->vol != 15) {
				fm_low_ops.bi.volset(15);
				fm->vol = 15;
				WCN_DBG(FM_NTC | MAIN, "Valid freq, volset: 15\n");
			}
		} else {
			if (fm->vol != 5) {
				fm_low_ops.bi.volset(5);
				fm->vol = 5;
				WCN_DBG(FM_NTC | MAIN, "Not a valid freq, volset: 5\n");
			}
		}
	}

	FM_UNLOCK(fm_ops_lock);
}
#endif

void fm_subsys_reset_work_func(struct work_struct *work)
{
	g_dbg_level = 0xffffffff;
	if (FM_LOCK(fm_ops_lock))
		return;

	fm_sys_state_set(g_fm_struct, FM_SUBSYS_RST_START);

	if (g_fm_struct->chipon == false) {
		WCN_DBG(FM_ALT | MAIN, "chip off no need do recover\n");
		goto out;
	}

	/* if whole chip reset, wmt will clear fm-on-flag, and firmware turn fm to off status,
	* so no need turn fm off again
	*/
	if (g_fm_struct->wholechiprst == false) {
		fm_low_ops.bi.pwrdownseq();
		/* subsystem power off */
		if (fm_low_ops.bi.pwroff(0)) {
			WCN_DBG(FM_ALT | MAIN, "chip off fail\n");
			goto out;
		}
	}

	/* subsystem power on */
	if (fm_low_ops.bi.pwron(0)) {
		WCN_DBG(FM_ALT | MAIN, "chip on fail\n");
		goto out;
	}
	/* recover context */
	if (g_fm_struct->chipon == false) {
		fm_low_ops.bi.pwroff(0);
		WCN_DBG(FM_ALT | MAIN, "no need do recover\n");
		goto out;
	}

	if (fm_pwr_state_get(g_fm_struct) == FM_PWR_RX_ON) {
		fm_low_ops.bi.pwrupseq(&g_fm_struct->chip_id, &g_fm_struct->device_id);
	} else {
		WCN_DBG(FM_ALT | MAIN, "no need do re-powerup\n");
		goto out;
	}

	fm_low_ops.bi.anaswitch(g_fm_struct->ana_type);

	fm_low_ops.bi.setfreq(fm_cur_freq_get());

	fm_low_ops.bi.volset((unsigned char) g_fm_struct->vol);

	g_fm_struct->mute = 0;
	fm_low_ops.bi.mute(g_fm_struct->mute);

	if (fm_low_ops.ri.rds_bci_get) {
		fm_timer_sys->init(fm_timer_sys, fm_timer_func, (unsigned long)g_fm_struct, fm_low_ops.ri.rds_bci_get(),
				   0);
		WCN_DBG(FM_NTC | MAIN, "initial timer ok\n");
	} else {
		WCN_DBG(FM_NTC | MAIN, "initial timer fail!!!\n");
	}

#if (FM_INVALID_CHAN_NOISE_REDUCING)
	fm_cqi_check_timer->init(fm_cqi_check_timer, fm_cqi_check_timer_func,
				 (unsigned long)g_fm_struct, 1000, 0);
#endif
	g_fm_struct->rds_on = 1;
	fm_low_ops.ri.rds_onoff(g_fm_struct->pstRDSData, g_fm_struct->rds_on);

	WCN_DBG(FM_ALT | MAIN, "recover done\n");

out:
	fm_sys_state_set(g_fm_struct, FM_SUBSYS_RST_END);
	fm_sys_state_set(g_fm_struct, FM_SUBSYS_RST_OFF);
	g_fm_struct->wholechiprst = true;

	FM_UNLOCK(fm_ops_lock);
	g_dbg_level = 0xfffffff5;
}

void fm_pwroff_work_func(struct work_struct *work)
{
	fm_powerdown(g_fm_struct, 0);
}

static void fm_eint_handler(void)
{
	struct fm *fm = g_fm_struct;

	WCN_DBG(FM_DBG | MAIN, "intr occur, ticks:%d\n", jiffies_to_msecs(jiffies));

	if (fm != NULL)
		fm->eint_wkthd->add_work(fm->eint_wkthd, fm->eint_wk);
}

signed int fm_rds_parser(struct rds_rx_t *rds_raw, signed int rds_size)
{
	struct fm *fm = g_fm_struct;	/* (struct fm *)work->data; */
	struct rds_t *pstRDSData = fm->pstRDSData;

	if (FM_LOCK(fm_read_lock))
		return -FM_ELOCK;
	/* parsing RDS data */
	fm_low_ops.ri.rds_parser(pstRDSData, rds_raw, rds_size, fm_cur_freq_get);
	FM_UNLOCK(fm_read_lock);

	if ((pstRDSData->event_status != 0x0000) && (pstRDSData->event_status != RDS_EVENT_AF_LIST)) {
		WCN_DBG(FM_NTC | MAIN, "Notify user to read, [event:0x%04x]\n", pstRDSData->event_status);
		FM_EVENT_SEND(fm->rds_event, FM_RDS_DATA_READY);
	}

	return 0;
}

static void fm_eint_work_func(struct work_struct *work)
{
	if (fm_wcn_ops.ei.eint_handler)
		fm_wcn_ops.ei.eint_handler();
	/* re-enable eint if need */
	fm_enable_eint();
}

static signed int fm_callback_register(struct fm_callback *cb)
{
	if (cb == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	cb->cur_freq_get = fm_cur_freq_get;
	cb->cur_freq_set = fm_cur_freq_set;
	cb->projectid_get = fm_projectid_get;
	return 0;
}

static signed int fm_ops_register(struct fm_lowlevel_ops *ops)
{
	signed int ret = 0;

	ret = fm_wcn_ops_register();
	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "fm_wcn_ops_register fail(%d)\n", ret);
		return ret;
	}

	ret = fm_callback_register(&ops->cb);
	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "fm_callback_register fail(%d)\n", ret);
		return ret;
	}

	if (fm_wcn_ops.ei.low_ops_register)
		ret = fm_wcn_ops.ei.low_ops_register(&ops->cb, &ops->bi);
	else
		ret = -1;
	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "fm_low_ops_register fail(%d)\n", ret);
		return ret;
	}

	if (fm_wcn_ops.ei.rds_ops_register)
		ret = fm_wcn_ops.ei.rds_ops_register(&ops->bi, &ops->ri);
	else
		ret = -1;
	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "rds_ops_register fail(%d)\n", ret);
		return ret;
	}

	return ret;
}
static signed int fm_callback_unregister(struct fm_callback *cb)
{
	if (cb == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,cb invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	fm_memset(cb, 0, sizeof(struct fm_callback));
	return 0;
}

static signed int fm_ops_unregister(struct fm_lowlevel_ops *ops)
{
	signed int ret = 0;

	if (fm_wcn_ops.ei.rds_ops_unregister)
		ret = fm_wcn_ops.ei.rds_ops_unregister(&ops->ri);
	else
		ret = -1;
	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "fm_rds_ops_unregister fail(%d)\n", ret);
		return ret;
	}

	if (fm_wcn_ops.ei.low_ops_unregister)
		ret = fm_wcn_ops.ei.low_ops_unregister(&ops->bi);
	else
		ret = -1;
	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "fm_low_ops_unregister fail(%d)\n", ret);
		return ret;
	}

	ret = fm_callback_unregister(&ops->cb);
	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "fm_callback_unregister fail(%d)\n", ret);
		return ret;
	}

	return ret;
}

static signed short fm_cust_config_setting(void)
{
	unsigned short chipid = 0;
	enum fm_cfg_chip_type type = FM_CHIP_TYPE_MAX;

	if (fm_low_ops.bi.chipid_get)
		chipid = fm_low_ops.bi.chipid_get();
	fm_which_chip(chipid, &type);
	WCN_DBG(FM_NTC | MAIN, "chipid:0x%x, chip type:%d\n", chipid, type);

	fm_cust_config_chip(chipid, type);
	/* init customer config parameter */
	fm_cust_config_setup(NULL);

	return 0;
}

struct fm *fm_dev_init(unsigned int arg)
{
	signed int ret = 0;
	struct fm *fm = NULL;

/* if (!fm_low_ops.ri.rds_bci_get) */
/* return NULL; */

	/* alloc fm main data structure */
	fm = fm_zalloc(sizeof(struct fm));
	if (!fm) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM\n");
		ret = -ENOMEM;
		return NULL;
	}

	fm->ref = 0;
	fm->chipon = false;
	fm_pwr_state_set(fm, FM_PWR_OFF);
	/* FM Tx */
	fm->vcoon = FM_TX_VCO_ON_DEFAULT;
	fm->vcooff = FM_TX_VCO_OFF_DEFAULT;
	fm->txpwrctl = FM_TX_PWR_CTRL_INVAL_DEFAULT;
	fm->tx_pwr = FM_TX_PWR_LEVEL_MAX;
	fm->wholechiprst = true;
	gps_rtc_info.err = 0;
	gps_rtc_info.age = 0;
	gps_rtc_info.drift = 0;
	gps_rtc_info.tv.tv_sec = 0;
	gps_rtc_info.tv.tv_usec = 0;
	gps_rtc_info.ageThd = FM_GPS_RTC_AGE_TH;
	gps_rtc_info.driftThd = FM_GPS_RTC_DRIFT_TH;
	gps_rtc_info.tvThd.tv_sec = FM_GPS_RTC_TIME_DIFF_TH;
	gps_rtc_info.retryCnt = FM_GPS_RTC_RETRY_CNT;
	gps_rtc_info.flag = FM_GPS_RTC_INFO_OLD;

	fm->rds_event = fm_flag_event_create("fm_rds_event");
	if (!fm->rds_event) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM for RDS event\n");
		ret = -ENOMEM;
		goto ERR_EXIT;
	}

	fm_flag_event_get(fm->rds_event);

	/* alloc fm rds data structure */
	fm->pstRDSData = fm_zalloc(sizeof(struct rds_t));
	if (!fm->pstRDSData) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM for RDS\n");
		ret = -ENOMEM;
		goto ERR_EXIT;
	}

	g_fm_struct = fm;

	fm->timer_wkthd = fm_workthread_create("fm_timer_wq");

	if (!fm->timer_wkthd) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM for fm_timer_wq\n");
		ret = -ENOMEM;
		goto ERR_EXIT;
	}

	fm_workthread_get(fm->timer_wkthd);

	fm->eint_wkthd = fm_workthread_create("fm_eint_wq");

	if (!fm->eint_wkthd) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM for fm_eint_wq\n");
		ret = -ENOMEM;
		goto ERR_EXIT;
	}

	fm_workthread_get(fm->eint_wkthd);

	fm->eint_wk = fm_work_create("fm_eint_work");

	if (!fm->eint_wk) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM for eint_wk\n");
		ret = -ENOMEM;
		goto ERR_EXIT;
	} else {
		fm_work_get(fm->eint_wk);
		fm->eint_wk->init(fm->eint_wk, fm_eint_work_func, (unsigned long)fm);
	}

	/* create reset work */
	fm->rst_wk = fm_work_create("fm_rst_work");

	if (!fm->rst_wk) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM for rst_wk\n");
		ret = -ENOMEM;
		goto ERR_EXIT;
	} else {
		fm_work_get(fm->rst_wk);
		fm->rst_wk->init(fm->rst_wk, fm_subsys_reset_work_func, (unsigned long)fm);
	}

	fm->rds_wk = fm_work_create("fm_rds_work");
	if (!fm->rds_wk) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM for rds_wk\n");
		ret = -ENOMEM;
		goto ERR_EXIT;
	} else {
		fm_work_get(fm->rds_wk);
		fm->rds_wk->init(fm->rds_wk, fm_rds_reset_work_func, (unsigned long)fm);
	}

#if (FM_INVALID_CHAN_NOISE_REDUCING)
	fm->ch_valid_check_wk = fm_work_create("fm_ch_valid_check_work");
	if (!fm->ch_valid_check_wk) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM for ch_valid_check_wk\n");
		ret = -ENOMEM;
		goto ERR_EXIT;
	} else {
		fm_work_get(fm->ch_valid_check_wk);
		fm->ch_valid_check_wk->init(fm->ch_valid_check_wk,
			fm_cqi_check_work_func, (unsigned long)fm);
	}
#endif

	fm->fm_tx_power_ctrl_work = fm_work_create("tx_pwr_ctl_work");
	if (!fm->fm_tx_power_ctrl_work) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM for tx_pwr_ctl_work\n");
		ret = -ENOMEM;
		goto ERR_EXIT;
	} else {
		fm_work_get(fm->fm_tx_power_ctrl_work);
		fm->fm_tx_power_ctrl_work->init(fm->fm_tx_power_ctrl_work,
						fm_tx_power_ctrl_worker_func, (unsigned long)fm);
	}

	fm->fm_tx_desense_wifi_work = fm_work_create("tx_desen_wifi_work");
	if (!fm->fm_tx_desense_wifi_work) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM for tx_desen_wifi_work\n");
		ret = -ENOMEM;
		goto ERR_EXIT;
	} else {
		fm_work_get(fm->fm_tx_desense_wifi_work);
		fm->fm_tx_desense_wifi_work->init(fm->fm_tx_desense_wifi_work,
						  fm_tx_desense_wifi_worker_func, (unsigned long)fm);
	}

	fm->pwroff_wk = fm_work_create("fm_pwroff_work");

	if (!fm->pwroff_wk) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM for pwroff_wk\n");
		ret = -ENOMEM;
		goto ERR_EXIT;
	} else {
		fm_work_get(fm->pwroff_wk);
		fm->pwroff_wk->init(fm->pwroff_wk, fm_pwroff_work_func, (unsigned long)fm);
	}

	/* fm timer was created in fm_env_setp() */
/* fm_timer_sys->init(fm_timer_sys, fm_timer_func, (unsigned long)g_fm_struct, fm_low_ops.ri.rds_bci_get(), 0); */
/* fm_timer_sys->start(fm_timer_sys); */
	fm_cust_config_setting();

	return g_fm_struct;

ERR_EXIT:

	if (!fm) {
		WCN_DBG(FM_NTC | MAIN, "fm is null\n");
		return NULL;
	}

	if (fm->eint_wkthd) {
		ret = fm_workthread_put(fm->eint_wkthd);
		if (!ret)
			fm->eint_wkthd = NULL;
	}

	if (fm->timer_wkthd) {
		ret = fm_workthread_put(fm->timer_wkthd);
		if (!ret)
			fm->timer_wkthd = NULL;
	}

	if (fm->eint_wk) {
		ret = fm_work_put(fm->eint_wk);
		if (!ret)
			fm->eint_wk = NULL;
	}

	if (fm->rds_wk) {
		ret = fm_work_put(fm->rds_wk);
		if (!ret)
			fm->rds_wk = NULL;
	}

	if (fm->ch_valid_check_wk) {
		ret = fm_work_put(fm->ch_valid_check_wk);
		if (!ret)
			fm->ch_valid_check_wk = NULL;
	}

	if (fm->rst_wk) {
		ret = fm_work_put(fm->rst_wk);
		if (!ret)
			fm->rst_wk = NULL;
	}

	if (fm->fm_tx_desense_wifi_work) {
		ret = fm_work_put(fm->fm_tx_desense_wifi_work);
		if (!ret)
			fm->fm_tx_desense_wifi_work = NULL;
	}

	if (fm->fm_tx_power_ctrl_work) {
		ret = fm_work_put(fm->fm_tx_power_ctrl_work);
		if (!ret)
			fm->fm_tx_power_ctrl_work = NULL;
	}

	if (fm->pwroff_wk) {
		ret = fm_work_put(fm->pwroff_wk);
		if (!ret)
			fm->pwroff_wk = NULL;
	}

	if (fm->pstRDSData) {
		fm_free(fm->pstRDSData);
		fm->pstRDSData = NULL;
	}

	fm_free(fm);
	g_fm_struct = NULL;
	return NULL;
}

signed int fm_dev_destroy(struct fm *fm)
{
	signed int ret = 0;

	WCN_DBG(FM_DBG | MAIN, "%s\n", __func__);

	fm_timer_sys->stop(fm_timer_sys);
#if (FM_INVALID_CHAN_NOISE_REDUCING)
	fm_timer_sys->stop(fm_cqi_check_timer);
#endif
	if (!fm) {
		WCN_DBG(FM_NTC | MAIN, "fm is null\n");
		return -1;
	}

	if (fm->eint_wkthd) {
		ret = fm_workthread_put(fm->eint_wkthd);
		if (!ret)
			fm->eint_wkthd = NULL;
	}

	if (fm->timer_wkthd) {
		ret = fm_workthread_put(fm->timer_wkthd);
		if (!ret)
			fm->timer_wkthd = NULL;
	}

	if (fm->eint_wk) {
		ret = fm_work_put(fm->eint_wk);
		if (!ret)
			fm->eint_wk = NULL;
	}

	if (fm->rds_wk) {
		ret = fm_work_put(fm->rds_wk);
		if (!ret)
			fm->rds_wk = NULL;
	}

	if (fm->ch_valid_check_wk) {
		ret = fm_work_put(fm->ch_valid_check_wk);
		if (!ret)
			fm->ch_valid_check_wk = NULL;
	}

	if (fm->rst_wk) {
		ret = fm_work_put(fm->rst_wk);
		if (!ret)
			fm->rst_wk = NULL;
	}

	if (fm->pwroff_wk) {
		ret = fm_work_put(fm->pwroff_wk);
		if (!ret)
			fm->pwroff_wk = NULL;
	}

	if (fm->pstRDSData) {
		fm_free(fm->pstRDSData);
		fm->pstRDSData = NULL;
	}

	if (fm->pstRDSData) {
		fm_free(fm->pstRDSData);
		fm->pstRDSData = NULL;
	}

	fm_flag_event_put(fm->rds_event);

	/* free all memory */
	if (fm) {
		fm_free(fm);
		fm = NULL;
		g_fm_struct = NULL;
	}

	return ret;
}

signed int fm_env_setup(void)
{
	signed int ret = 0;

	WCN_DBG(FM_NTC | MAIN, "%s\n", __func__);

	ret = fm_ops_register(&fm_low_ops);
	if (ret)
		return ret;

	WCN_DBG(FM_NTC | MAIN, "fm ops registered\n");


	fm_ops_lock = fm_lock_create("ops_lock");
	if (!fm_ops_lock)
		return -1;

	fm_read_lock = fm_lock_create("rds_read");
	if (!fm_read_lock)
		return -1;

	fm_rds_cnt = fm_lock_create("rds_cnt");
	if (!fm_rds_cnt)
		return -1;

	fm_timer_lock = fm_spin_lock_create("timer_lock");
	if (!fm_timer_lock)
		return -1;

	fm_rxtx_lock = fm_lock_create("rxtx_lock");
	if (!fm_rxtx_lock)
		return -1;

	fm_rtc_mutex = fm_lock_create("rtc_lock");
	if (!fm_rxtx_lock)
		return -1;

	fm_wcn_ops.tx_lock = fm_lock_create("tx_lock");
	if (!fm_wcn_ops.tx_lock)
		return -1;

	fm_wcn_ops.own_lock = fm_lock_create("own_lock");
	if (!fm_wcn_ops.own_lock)
		return -1;

	fm_lock_get(fm_ops_lock);
	fm_lock_get(fm_read_lock);
	fm_lock_get(fm_rds_cnt);
	fm_spin_lock_get(fm_timer_lock);
	fm_lock_get(fm_rxtx_lock);
	fm_lock_get(fm_rtc_mutex);
	fm_lock_get(fm_wcn_ops.tx_lock);
	fm_lock_get(fm_wcn_ops.own_lock);
	WCN_DBG(FM_NTC | MAIN, "fm locks created\n");

	fm_timer_sys = fm_timer_create("fm_sys_timer");

	if (!fm_timer_sys)
		return -1;

	fm_timer_get(fm_timer_sys);

#if (FM_INVALID_CHAN_NOISE_REDUCING)
	fm_cqi_check_timer = fm_timer_create("fm_cqi_check_timer");
	if (!fm_cqi_check_timer)
		return -1;
	fm_timer_get(fm_cqi_check_timer);
	fm_cqi_check_timer->init(fm_cqi_check_timer, fm_cqi_check_timer_func, (unsigned long)g_fm_struct, 500, 0);
#endif

	WCN_DBG(FM_NTC | MAIN, "fm timer created\n");

	ret = fm_link_setup((void *)fm_wholechip_rst_cb);

	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "fm link setup Failed\n");
		return -1;
	}

	return ret;
}

signed int fm_env_destroy(void)
{
	signed int ret = 0;

	WCN_DBG(FM_NTC | MAIN, "%s\n", __func__);

	fm_link_release();

	ret = fm_ops_unregister(&fm_low_ops);
	if (ret)
		return ret;

	WCN_DBG(FM_NTC | MAIN, "fm ops unregistered\n");

	ret = fm_lock_put(fm_ops_lock);
	if (!ret)
		fm_ops_lock = NULL;

	ret = fm_lock_put(fm_read_lock);
	if (!ret)
		fm_read_lock = NULL;

	ret = fm_lock_put(fm_rds_cnt);
	if (!ret)
		fm_rds_cnt = NULL;

	ret = fm_spin_lock_put(fm_timer_lock);
	if (!ret)
		fm_timer_lock = NULL;

	ret = fm_lock_put(fm_rxtx_lock);
	if (!ret)
		fm_rxtx_lock = NULL;

	ret = fm_lock_put(fm_rtc_mutex);
	if (!ret)
		fm_rtc_mutex = NULL;

	ret = fm_timer_put(fm_timer_sys);
	if (!ret)
		fm_timer_sys = NULL;

#if (FM_INVALID_CHAN_NOISE_REDUCING)
	ret = fm_timer_put(fm_cqi_check_timer);
	if (!ret)
		fm_cqi_check_timer = NULL;
#endif

	ret = fm_lock_put(fm_wcn_ops.tx_lock);
	if (!ret)
		fm_wcn_ops.tx_lock = NULL;
	ret = fm_lock_put(fm_wcn_ops.own_lock);
	if (!ret)
		fm_wcn_ops.own_lock = NULL;

	return ret;
}

/*
 * GetChannelSpace - get the spcace of gived channel
 * @freq - value in 760~1080 or 7600~10800
 *
 * Return 0, if 760~1080; return 1, if 7600 ~ 10800, else err code < 0
 */
signed int fm_get_channel_space(signed int freq)
{
	if ((freq >= 640) && (freq <= 1080))
		return 0;
	else if ((freq >= 6400) && (freq <= 10800))
		return 1;
	else
		return -1;
}
