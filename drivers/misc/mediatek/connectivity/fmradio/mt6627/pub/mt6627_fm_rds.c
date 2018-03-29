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

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_interface.h"
#include "fm_stdlib.h"
#include "fm_rds.h"
#include "mt6627_fm_reg.h"
#include "fm_cmd.h"

static fm_bool bRDS_FirstIn = fm_false;
static fm_u32 gBLER_CHK_INTERVAL = 5000;
static fm_u16 GOOD_BLK_CNT = 0, BAD_BLK_CNT;
static fm_u8 BAD_BLK_RATIO;

static struct fm_basic_interface *fm_bi;

static fm_bool mt6627_RDS_support(void);
static fm_s32 mt6627_RDS_enable(void);
static fm_s32 mt6627_RDS_disable(void);
static fm_u16 mt6627_RDS_Get_GoodBlock_Counter(void);
static fm_u16 mt6627_RDS_Get_BadBlock_Counter(void);
static fm_u8 mt6627_RDS_Get_BadBlock_Ratio(void);
static fm_u32 mt6627_RDS_Get_BlerCheck_Interval(void);
/* static void mt6627_RDS_GetData(fm_u16 *data, fm_u16 datalen); */
static void mt6627_RDS_Init_Data(rds_t *pstRDSData);

static fm_bool mt6627_RDS_support(void)
{
	return fm_true;
}

static fm_s32 mt6627_RDS_enable(void)
{
	fm_s32 ret = 0;
	fm_u16 dataRead = 0;

	WCN_DBG(FM_DBG | RDSC, "rds enable\n");
	ret = fm_reg_read(FM_RDS_CFG0, &dataRead);
	if (ret) {
		WCN_DBG(FM_NTC | RDSC, "rds enable read 0x80 fail\n");
		return ret;
	}
	ret = fm_reg_write(FM_RDS_CFG0, 6);	/* set buf_start_th */
	if (ret) {
		WCN_DBG(FM_NTC | RDSC, "rds enable write 0x80 fail\n");
		return ret;
	}
	ret = fm_reg_read(FM_MAIN_CTRL, &dataRead);
	if (ret) {
		WCN_DBG(FM_NTC | RDSC, "rds enable read 0x63 fail\n");
		return ret;
	}
	ret = fm_reg_write(FM_MAIN_CTRL, dataRead | (RDS_MASK));
	if (ret) {
		WCN_DBG(FM_NTC | RDSC, "rds enable write 0x63 fail\n");
		return ret;
	}

	return ret;
}

static fm_s32 mt6627_RDS_disable(void)
{
	fm_s32 ret = 0;
	fm_u16 dataRead = 0;

	WCN_DBG(FM_DBG | RDSC, "rds disable\n");
	ret = fm_reg_read(FM_MAIN_CTRL, &dataRead);
	if (ret) {
		WCN_DBG(FM_NTC | RDSC, "rds disable read 0x63 fail\n");
		return ret;
	}
	ret = fm_reg_write(FM_MAIN_CTRL, dataRead & (~RDS_MASK));
	if (ret) {
		WCN_DBG(FM_NTC | RDSC, "rds disable write 0x63 fail\n");
		return ret;
	}

	return ret;
}

static fm_u16 mt6627_RDS_Get_GoodBlock_Counter(void)
{
	fm_u16 tmp_reg;

	fm_reg_read(FM_RDS_GOODBK_CNT, &tmp_reg);
	GOOD_BLK_CNT = tmp_reg;
	WCN_DBG(FM_DBG | RDSC, "get good block cnt:%d\n", (fm_s32) tmp_reg);

	return tmp_reg;
}

static fm_u16 mt6627_RDS_Get_BadBlock_Counter(void)
{
	fm_u16 tmp_reg;

	fm_reg_read(FM_RDS_BADBK_CNT, &tmp_reg);
	BAD_BLK_CNT = tmp_reg;
	WCN_DBG(FM_DBG | RDSC, "get bad block cnt:%d\n", (fm_s32) tmp_reg);

	return tmp_reg;
}

static fm_u8 mt6627_RDS_Get_BadBlock_Ratio(void)
{
	fm_u16 tmp_reg;
	fm_u16 gbc;
	fm_u16 bbc;

	gbc = mt6627_RDS_Get_GoodBlock_Counter();
	bbc = mt6627_RDS_Get_BadBlock_Counter();

	if ((gbc + bbc) > 0)
		tmp_reg = (fm_u8) (bbc * 100 / (gbc + bbc));
	else
		tmp_reg = 0;

	BAD_BLK_RATIO = tmp_reg;
	WCN_DBG(FM_DBG | RDSC, "get badblock ratio:%d\n", (fm_s32) tmp_reg);

	return tmp_reg;
}

static fm_s32 mt6627_RDS_BlockCounter_Reset(void)
{
	mt6627_RDS_disable();
	mt6627_RDS_enable();

	return 0;
}

static fm_u32 mt6627_RDS_Get_BlerCheck_Interval(void)
{
	return gBLER_CHK_INTERVAL;
}

static fm_s32 mt6627_RDS_BlerCheck(rds_t *dst)
{
	return 0;
}

#if 0
static void RDS_Recovery_Handler(void)
{
	fm_u16 tempData = 0;

	do {
		fm_reg_read(FM_RDS_DATA_REG, &tempData);
		fm_reg_read(FM_RDS_POINTER, &tempData);
	} while (tempData & 0x3);
}
#endif

#if 0
static void mt6627_RDS_GetData(fm_u16 *data, fm_u16 datalen)
{
#define RDS_GROUP_DIFF_OFS          0x007C
#define RDS_FIFO_DIFF               0x007F
#define RDS_CRC_BLK_ADJ             0x0020
#define RDS_CRC_CORR_CNT            0x001E
#define RDS_CRC_INFO                0x0001

	fm_u16 CRC = 0, i = 0, RDS_adj = 0, RDSDataCount = 0, FM_WARorrCnt = 0;
	fm_u16 temp = 0, OutputPofm_s32 = 0;

	WCN_DBG(FM_DBG | RDSC, "get data\n");
	fm_reg_read(FM_RDS_FIFO_STATUS0, &temp);
	RDSDataCount = ((RDS_GROUP_DIFF_OFS & temp) << 2);

	if ((temp & RDS_FIFO_DIFF) >= 4) {
		/* block A data and info handling */
		fm_reg_read(FM_RDS_INFO, &temp);
		RDS_adj |= (temp & RDS_CRC_BLK_ADJ) << 10;
		CRC |= (temp & RDS_CRC_INFO) << 3;
		FM_WARorrCnt |= ((temp & RDS_CRC_CORR_CNT) << 11);
		fm_reg_read(FM_RDS_DATA_REG, &data[0]);

		/* block B data and info handling */
		fm_reg_read(FM_RDS_INFO, &temp);
		RDS_adj |= (temp & RDS_CRC_BLK_ADJ) << 9;
		CRC |= (temp & RDS_CRC_INFO) << 2;
		FM_WARorrCnt |= ((temp & RDS_CRC_CORR_CNT) << 7);
		fm_reg_read(FM_RDS_DATA_REG, &data[1]);

		/* block C data and info handling */
		fm_reg_read(FM_RDS_INFO, &temp);
		RDS_adj |= (temp & RDS_CRC_BLK_ADJ) << 8;
		CRC |= (temp & RDS_CRC_INFO) << 1;
		FM_WARorrCnt |= ((temp & RDS_CRC_CORR_CNT) << 3);
		fm_reg_read(FM_RDS_DATA_REG, &data[2]);

		/* block D data and info handling */
		fm_reg_read(FM_RDS_INFO, &temp);
		RDS_adj |= (temp & RDS_CRC_BLK_ADJ) << 7;
		CRC |= (temp & RDS_CRC_INFO);
		FM_WARorrCnt |= ((temp & RDS_CRC_CORR_CNT) >> 1);
		fm_reg_read(FM_RDS_DATA_REG, &data[3]);

		data[4] = (CRC | RDS_adj | RDSDataCount);
		data[5] = FM_WARorrCnt;

		fm_reg_read(FM_RDS_PWDI, &data[6]);
		fm_reg_read(FM_RDS_PWDQ, &data[7]);

		fm_reg_read(FM_RDS_POINTER, &OutputPofm_s32);

		/* Go fm_s32o RDS recovery handler while RDS output pofm_s32 doesn't align to 4 in numeric */
		if (OutputPofm_s32 & 0x3)
			RDS_Recovery_Handler();

	} else {
		for (; i < 8; i++)
			data[i] = 0;
	}
}
#endif

static void mt6627_RDS_Init_Data(rds_t *pstRDSData)
{
	fm_memset(pstRDSData, 0, sizeof(rds_t));
	bRDS_FirstIn = fm_true;

	pstRDSData->event_status = 0x0000;
	fm_memset(pstRDSData->RT_Data.TextData, 0x20, sizeof(pstRDSData->RT_Data.TextData));
	fm_memset(pstRDSData->PS_Data.PS, '\0', sizeof(pstRDSData->PS_Data.PS));
	fm_memset(pstRDSData->PS_ON, 0x20, sizeof(pstRDSData->PS_ON));
}

fm_bool mt6627_RDS_OnOff(rds_t *dst, fm_bool bFlag)
{
	fm_s32 ret = 0;

	if (mt6627_RDS_support() == fm_false) {
		WCN_DBG(FM_ALT | RDSC, "mt6627_RDS_OnOff failed, RDS not support\n");
		return fm_false;
	}

	if (bFlag) {
		mt6627_RDS_Init_Data(dst);
		ret = mt6627_RDS_enable();
		if (ret) {
			WCN_DBG(FM_NTC | RDSC, "mt6630_RDS_OnOff enable failed\n");
			return fm_false;
		}
	} else {
		mt6627_RDS_Init_Data(dst);
		ret = mt6627_RDS_disable();
		if (ret) {
			WCN_DBG(FM_NTC | RDSC, "mt6630_RDS_OnOff disable failed\n");
			return fm_false;
		}
	}

	return fm_true;
}

DEFINE_RDSLOG(mt6627_rds_log);

/* mt6627_RDS_Efm_s32_Handler    -    response FM RDS interrupt
 * @fm - main data structure of FM driver
 * This function first get RDS raw data, then call RDS spec parser
 */
static fm_s32 mt6627_rds_parser(rds_t *rds_dst, struct rds_rx_t *rds_raw, fm_s32 rds_size, fm_u16(*getfreq) (void))
{
	mt6627_rds_log.log_in(&mt6627_rds_log, rds_raw, rds_size);
	return rds_parser(rds_dst, rds_raw, rds_size, getfreq);
}

static fm_s32 mt6627_rds_log_get(struct rds_rx_t *dst, fm_s32 *dst_len)
{
	return mt6627_rds_log.log_out(&mt6627_rds_log, dst, dst_len);
}

static fm_s32 mt6627_rds_gc_get(struct rds_group_cnt_t *dst, rds_t *rdsp)
{
	return rds_grp_counter_get(dst, &rdsp->gc);
}

static fm_s32 mt6627_rds_gc_reset(rds_t *rdsp)
{
	return rds_grp_counter_reset(&rdsp->gc);
}

fm_s32 fm_rds_ops_register(struct fm_basic_interface *bi, struct fm_rds_interface *ri)
{
	fm_s32 ret = 0;

	if (ri == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,ri invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	fm_bi = bi;

	ri->rds_blercheck = mt6627_RDS_BlerCheck;
	ri->rds_onoff = mt6627_RDS_OnOff;
	ri->rds_parser = mt6627_rds_parser;
	ri->rds_gbc_get = mt6627_RDS_Get_GoodBlock_Counter;
	ri->rds_bbc_get = mt6627_RDS_Get_BadBlock_Counter;
	ri->rds_bbr_get = mt6627_RDS_Get_BadBlock_Ratio;
	ri->rds_bc_reset = mt6627_RDS_BlockCounter_Reset;
	ri->rds_bci_get = mt6627_RDS_Get_BlerCheck_Interval;
	ri->rds_log_get = mt6627_rds_log_get;
	ri->rds_gc_get = mt6627_rds_gc_get;
	ri->rds_gc_reset = mt6627_rds_gc_reset;
	return ret;
}

fm_s32 fm_rds_ops_unregister(struct fm_rds_interface *ri)
{
	fm_s32 ret = 0;

	if (ri == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,ri invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	fm_bi = NULL;
	fm_memset(ri, 0, sizeof(struct fm_rds_interface));
	return ret;
}
