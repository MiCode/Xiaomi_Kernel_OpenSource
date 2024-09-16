/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

    Module Name:
    dvt_dmashdl.c

    Abstract:
    For DMA sheduler DVT.
    Please refer to DVT plan of DMA SCHEDULER for details

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
    Kai         2019/01/28      develop this for MT7915 USB(WA)
*/
#include "precomp.h"

#if (CFG_SUPPORT_DMASHDL_SYSDVT)
/*
* This routine is used to test return page rule
*/
void dmashdl_dvt_item_6(
	struct GLUE_INFO *prGlueInfo,
	struct DMASHDL_DVT_CMD_T *tDvtCmd)
{
	uint32_t value;
	struct ADAPTER *prAdapter = NULL;
#if (CFG_SUPPORT_CONNAC2X == 1)
	struct mt66xx_chip_info *prChipInfo;
#endif
	prAdapter = prGlueInfo->prAdapter;

	if (tDvtCmd->ucSubItemNo == DMASHDL_DVT_SUBITEM_1) {
		if (!AutomationInit(prAdapter, DMASHDL))
			return;

		/* reset all setting and count */
		HIF_RESET_SW_LOGIC(prGlueInfo);

		/* mapping all queue to Group 0~4 */
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_QUEUE_MAPPING0_ADDR,
			0x44443210);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_QUEUE_MAPPING1_ADDR,
			0x44444444);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_QUEUE_MAPPING2_ADDR,
			0x44444444);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_QUEUE_MAPPING3_ADDR,
			0x44444444);

		/* set quota, max quota 0x4, min quota 0x2*/
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR,
			0x00040002);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP1_CONTROL_ADDR,
			0x00040002);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP2_CONTROL_ADDR,
			0x00040002);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP3_CONTROL_ADDR,
			0x00040002);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP4_CONTROL_ADDR,
			0x00040002);

		/* enable refill */
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_REFILL_CONTROL_ADDR,
			0xffe00000);

		/* maximux page */
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR,
			0x10);

		/* enable WA CPU mode */
		kalDevRegRead(prGlueInfo,
			WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
			&value);
		value = value |
			WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_CR_WACPU_MODE_EN_MASK;
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
			value);

#if (CFG_SUPPORT_CONNAC2X == 1)
		/* inform WACPU test item */
		prChipInfo = prAdapter->chip_info;
		if (prChipInfo->is_support_wacpu)
			CmdExtDmaShdlDvt2WA(prAdapter,
				tDvtCmd->ucItemNo,
				tDvtCmd->ucSubItemNo);
#endif /* CFG_SUPPORT_CONNAC2X */

		/* Step 1. init RSV & SRC */
		HIF_ADD_RSV_CNT(prGlueInfo, 0, 0x2);
		HIF_ADD_SRC_CNT(prGlueInfo, 0, 0x3);
		HIF_ADD_SRC_CNT(prGlueInfo, 1, 0x4);
		HIF_ADD_RSV_CNT(prGlueInfo, 2, 0x1);
		HIF_ADD_SRC_CNT(prGlueInfo, 2, 0x2);
		HIF_ADD_SRC_CNT(prGlueInfo, 3, 0x2);
		HIF_ADD_SRC_CNT(prGlueInfo, 4, 0x2);
		HIF_CPU_RTN_CNT(prGlueInfo, 4, 0x2);

		DMASHDL_DVT_SET_ITEM(prAdapter, DMASHDL_DVT_ITEM_6);
	} else if (tDvtCmd->ucSubItemNo == DMASHDL_DVT_SUBITEM_2) {
		/* step 2. return 3 free pages to Group 0 */
		HIF_SUB_SRC_CNT(prGlueInfo, 0, 0x3);
		HIF_ADD_RSV_CNT(prGlueInfo, 0, 0x3);
	}
}

/*
* This routine is used to test FFA refill
*/
void dmashdl_dvt_item_5(
	struct GLUE_INFO *prGlueInfo,
	struct DMASHDL_DVT_CMD_T *tDvtCmd)
{
	uint32_t value;
	struct ADAPTER *prAdapter = NULL;
#if (CFG_SUPPORT_CONNAC2X == 1)
	struct mt66xx_chip_info *prChipInfo;
#endif
	prAdapter = prGlueInfo->prAdapter;

	if (!AutomationInit(prAdapter, DMASHDL))
		return;

	/* reset all setting and count */
	HIF_RESET_SW_LOGIC(prGlueInfo);

	/* mapping all queue to Group 0~4 */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING0_ADDR,
		0x44443210);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING1_ADDR,
		0x44444444);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING2_ADDR,
		0x44444444);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING3_ADDR,
		0x44444444);

	if (tDvtCmd->ucSubItemNo == DMASHDL_DVT_SUBITEM_2) {
		/* set quota, min quota 0x5 for Group 0 & 1 */
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR,
			0x0fff0005);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP1_CONTROL_ADDR,
			0x0fff0005);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP2_CONTROL_ADDR,
			0x0fff0001);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP3_CONTROL_ADDR,
			0x0fff0002);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP4_CONTROL_ADDR,
			0x0fff0001);
	} else {
		/* set quota, min quota 0x5 for Group1*/
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR,
			0x0fff0001);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP1_CONTROL_ADDR,
			0x0fff0005);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP2_CONTROL_ADDR,
			0x0fff0001);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP3_CONTROL_ADDR,
			0x0fff0001);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP4_CONTROL_ADDR,
			0x0fff0001);
	}

	/* enable refill */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_REFILL_CONTROL_ADDR,
		0xffe00000);

	/* maximux page */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR,
		0x10);

	/* enable WA CPU mode */
	kalDevRegRead(prGlueInfo,
		WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
		&value);
	value = value |
		WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_CR_WACPU_MODE_EN_MASK;
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
		value);

#if (CFG_SUPPORT_CONNAC2X == 1)
	/* inform WACPU test item */
	prChipInfo = prAdapter->chip_info;
	if (prChipInfo->is_support_wacpu)
		CmdExtDmaShdlDvt2WA(prAdapter,
			tDvtCmd->ucItemNo,
			tDvtCmd->ucSubItemNo);
#endif /* CFG_SUPPORT_CONNAC2X */

	/* init quota */
	HIF_ADD_RSV_CNT(prGlueInfo, 0, 0x13);

	/* maximux page, start to move packet */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR,
		0x1);

	DMASHDL_DVT_RESET(prAdapter);
	DMASHDL_DVT_SET_SUBITEM(prAdapter, tDvtCmd->ucSubItemNo);
	DMASHDL_DVT_SET_ITEM(prAdapter, DMASHDL_DVT_ITEM_5);

	/* start to send ping packets to each group */
}

/*
* This routine is used to test slot priority
*/
void dmashdl_dvt_item_4(
	struct GLUE_INFO *prGlueInfo,
	struct DMASHDL_DVT_CMD_T *tDvtCmd)
{
	uint32_t value;
	struct ADAPTER *prAdapter = NULL;
#if (CFG_SUPPORT_CONNAC2X == 1)
	struct mt66xx_chip_info *prChipInfo;
#endif
	prAdapter = prGlueInfo->prAdapter;

	if (tDvtCmd->ucSubItemNo == DMASHDL_DVT_SUBITEM_1) {
		if (!AutomationInit(prAdapter, DMASHDL))
			return;

		/* reset all setting and count */
		HIF_RESET_SW_LOGIC(prGlueInfo);

		/* mapping all queue to Group 0~4 */
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_QUEUE_MAPPING0_ADDR,
			0x44443210);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_QUEUE_MAPPING1_ADDR,
			0x44444444);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_QUEUE_MAPPING2_ADDR,
			0x44444444);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_QUEUE_MAPPING3_ADDR,
			0x44444444);

		/* set quota, max quota 0x8, min quota 0x2*/
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR,
			0x0fff0001);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP1_CONTROL_ADDR,
			0x0fff0001);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP2_CONTROL_ADDR,
			0x0fff0001);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP3_CONTROL_ADDR,
			0x0fff0001);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP4_CONTROL_ADDR,
			0x0fff0001);

		/* enable refill */
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_REFILL_CONTROL_ADDR,
			0xffe00000);

		/* disable joint ASK RR */
		kalDevRegRead(prGlueInfo,
			WF_HIF_DMASHDL_TOP_OPTIONAL_CONTROL_ADDR,
			&value);
		value = (value &
~WF_HIF_DMASHDL_TOP_OPTIONAL_CONTROL_CR_HIF_GUP_ACT_MAP_MASK);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_OPTIONAL_CONTROL_ADDR,
			value);

		/* disable SRC_CNT_PRI_EN & */
		/* pre-define each slot group strict order(enable as default) */
		kalDevRegRead(prGlueInfo,
			WF_HIF_DMASHDL_TOP_PAGE_SETTING_ADDR,
			&value);
		value = (value &
~WF_HIF_DMASHDL_TOP_PAGE_SETTING_SRC_CNT_PRI_EN_MASK) |
WF_HIF_DMASHDL_TOP_PAGE_SETTING_GROUP_SEQUENCE_ORDER_TYPE_MASK;
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_PAGE_SETTING_ADDR,
			value);

		/* maximux page */
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR,
			0x10);

		/* enable WA CPU mode */
		kalDevRegRead(prGlueInfo,
			WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
			&value);
		value = value |
WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_CR_WACPU_MODE_EN_MASK;
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
			value);

#if (CFG_SUPPORT_CONNAC2X == 1)
		/* inform WACPU test item */
		prChipInfo = prAdapter->chip_info;
		if (prChipInfo->is_support_wacpu)
			CmdExtDmaShdlDvt2WA(prAdapter,
				tDvtCmd->ucItemNo,
				tDvtCmd->ucSubItemNo);
#endif /* CFG_SUPPORT_CONNAC2X */

		/* init quota */
		HIF_ADD_RSV_CNT(prGlueInfo, 0, 0x13);

		DMASHDL_DVT_RESET(prAdapter);
		DMASHDL_DVT_SET_ITEM(prAdapter, DMASHDL_DVT_ITEM_4);

		/* start to send ping packets to each group */
	} else if (tDvtCmd->ucSubItemNo == DMASHDL_DVT_SUBITEM_2) {
		/* maximux page, start to move packet */
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR,
			0x1);
	}
}

/*
* This routine is used to test group priority
*/
void dmashdl_dvt_item_3(
	struct GLUE_INFO *prGlueInfo,
	struct DMASHDL_DVT_CMD_T *tDvtCmd)
{
	uint32_t value;
	struct ADAPTER *prAdapter = NULL;
#if (CFG_SUPPORT_CONNAC2X == 1)
	struct mt66xx_chip_info *prChipInfo;
#endif
	prAdapter = prGlueInfo->prAdapter;

	if (tDvtCmd->ucSubItemNo == DMASHDL_DVT_SUBITEM_1) {
		if (!AutomationInit(prAdapter, DMASHDL))
			return;

		/* reset all setting and count */
		HIF_RESET_SW_LOGIC(prGlueInfo);

		/* mapping all queue to Group 0~4 */
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_QUEUE_MAPPING0_ADDR,
			0x44443210);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_QUEUE_MAPPING1_ADDR,
			0x44444444);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_QUEUE_MAPPING2_ADDR,
			0x44444444);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_QUEUE_MAPPING3_ADDR,
			0x44444444);

		/* set quota, max quota 0x8, min quota 0x2*/
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR,
			0x0fff0001);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP1_CONTROL_ADDR,
			0x0fff0001);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP2_CONTROL_ADDR,
			0x0fff0001);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP3_CONTROL_ADDR,
			0x0fff0001);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_GROUP4_CONTROL_ADDR,
			0x0fff0001);

		/* enable refill */
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_REFILL_CONTROL_ADDR,
			0xffe00000);

		/* disable joint ASK RR */
		kalDevRegRead(prGlueInfo,
			WF_HIF_DMASHDL_TOP_OPTIONAL_CONTROL_ADDR,
			&value);
		value = value &
~WF_HIF_DMASHDL_TOP_OPTIONAL_CONTROL_CR_HIF_GUP_ACT_MAP_MASK;
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_OPTIONAL_CONTROL_ADDR,
			value);

/* disable SRC_CNT_PRI_EN & user program group sequence order type */
		kalDevRegRead(prGlueInfo,
			WF_HIF_DMASHDL_TOP_PAGE_SETTING_ADDR,
			&value);
		value = value &
~WF_HIF_DMASHDL_TOP_PAGE_SETTING_SRC_CNT_PRI_EN_MASK &
~WF_HIF_DMASHDL_TOP_PAGE_SETTING_GROUP_SEQUENCE_ORDER_TYPE_MASK;
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_PAGE_SETTING_ADDR,
			value);

		/* group priority */
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_HIF_SCHEDULER_SETTING0_ADDR,
			0x76540123);
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_HIF_SCHEDULER_SETTING1_ADDR,
			0xfedcba98);

		/* maximux page */
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR,
			0x10);

		/* enable WA CPU mode */
		kalDevRegRead(prGlueInfo,
			WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
			&value);
		value = value |
			WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_CR_WACPU_MODE_EN_MASK;
		kalDevRegWrite(prGlueInfo,
			WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
			value);

#if (CFG_SUPPORT_CONNAC2X == 1)
		/* inform WACPU test item */
		prChipInfo = prAdapter->chip_info;
		if (prChipInfo->is_support_wacpu)
			CmdExtDmaShdlDvt2WA(prAdapter,
				tDvtCmd->ucItemNo,
				tDvtCmd->ucSubItemNo);
#endif /* CFG_SUPPORT_CONNAC2X */

		/* init quota */
		HIF_ADD_RSV_CNT(prGlueInfo, 0, 0x13);

		DMASHDL_DVT_RESET(prAdapter);
		DMASHDL_DVT_SET_ITEM(prAdapter, DMASHDL_DVT_ITEM_3);

		/* start to send ping packets to each group */
	} else if (tDvtCmd->ucSubItemNo == DMASHDL_DVT_SUBITEM_2) {
		/* maximux page, start to move packet */
		kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR,
		0x1);
	}
}

/*
* This routine is used to run stress test
*/
void dmashdl_dvt_item_2(
	struct GLUE_INFO *prGlueInfo,
	struct DMASHDL_DVT_CMD_T *tDvtCmd)
{
	uint32_t value;
#if (CFG_SUPPORT_CONNAC2X == 1)
	struct mt66xx_chip_info *prChipInfo;
#endif
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;

	if (!AutomationInit(prAdapter, DMASHDL))
		return;

	/* reset all setting and count */
	HIF_RESET_SW_LOGIC(prGlueInfo);

	/* mapping all queue to Group 0~4 */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING0_ADDR,
		0x21043210);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING1_ADDR,
		0x04321043);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING2_ADDR,
		0x32104321);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING3_ADDR,
		0x10432104);

	/* set quota, max quota 0x8, min quota 0x2*/
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR,
		0x00080002);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP1_CONTROL_ADDR,
		0x00080002);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP2_CONTROL_ADDR,
		0x00080002);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP3_CONTROL_ADDR,
		0x00080002);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP4_CONTROL_ADDR,
		0x00080002);

	/* enable refill */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_REFILL_CONTROL_ADDR,
		0xffe00000);

	/* maximux page */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR,
		0x10);

	/* enable WA CPU mode */
	kalDevRegRead(prGlueInfo,
		WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
		&value);
	value = value |
		WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_CR_WACPU_MODE_EN_MASK;
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
		value);

#if (CFG_SUPPORT_CONNAC2X == 1)
	/* inform WACPU test item */
	prChipInfo = prAdapter->chip_info;
	if (prChipInfo->is_support_wacpu)
		CmdExtDmaShdlDvt2WA(prAdapter,
			tDvtCmd->ucItemNo,
			tDvtCmd->ucSubItemNo);
#endif /* CFG_SUPPORT_CONNAC2X */

	/* init quota */
	HIF_ADD_RSV_CNT(prGlueInfo, 0, 0x13);

	/* maximux page, start to move packet */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR,
		0x1);

	DMASHDL_DVT_RESET(prAdapter);
	DMASHDL_DVT_SET_ITEM(prAdapter, DMASHDL_DVT_ITEM_2);

	/* start to send ping packets to each group */
}

/*
* This routine is used to test flow_control for Group 0 ~ 4
*/
void dmashdl_dvt_item_1(
	struct GLUE_INFO *prGlueInfo,
	struct DMASHDL_DVT_CMD_T *tDvtCmd)
{
	uint32_t value;
	struct ADAPTER *prAdapter = NULL;
#if (CFG_SUPPORT_CONNAC2X == 1)
	struct mt66xx_chip_info *prChipInfo;
#endif
	prAdapter = prGlueInfo->prAdapter;

	if (!AutomationInit(prAdapter, DMASHDL))
		return;

	/* reset all setting and count */
	HIF_RESET_SW_LOGIC(prGlueInfo);

	/* mapping all queue to Group 0~4 */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING0_ADDR,
		0x21043210);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING1_ADDR,
		0x04321043);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING2_ADDR,
		0x32104321);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING3_ADDR,
		0x10432104);

	/* set quota, max quota 0xfff, min quota 0x4*/
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR,
		0x0fff0004);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP1_CONTROL_ADDR,
		0x0fff0004);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP2_CONTROL_ADDR,
		0x0fff0004);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP3_CONTROL_ADDR,
		0x0fff0004);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP4_CONTROL_ADDR,
		0x0fff0004);

	/* disable refill */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_REFILL_CONTROL_ADDR,
		0x0);

	/* maximux page */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR,
		0x10);

	/* enable WA CPU mode */
	kalDevRegRead(prGlueInfo,
		WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
		&value);
	value = value |
		WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_CR_WACPU_MODE_EN_MASK;
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
		value);

	/* tell WA CPU don't return pages */
#if (CFG_SUPPORT_CONNAC2X == 1)
	/* inform WACPU test item */
	prChipInfo = prAdapter->chip_info;
	if (prChipInfo->is_support_wacpu)
		CmdExtDmaShdlDvt2WA(prAdapter,
			tDvtCmd->ucItemNo,
			tDvtCmd->ucSubItemNo);
#endif /* CFG_SUPPORT_CONNAC2X */

	/* init quota */
	HIF_ADD_RSV_CNT(prGlueInfo, 0, 0x4);
	HIF_ADD_RSV_CNT(prGlueInfo, 1, 0x4);
	HIF_ADD_RSV_CNT(prGlueInfo, 2, 0x4);
	HIF_ADD_RSV_CNT(prGlueInfo, 3, 0x4);
	HIF_ADD_RSV_CNT(prGlueInfo, 4, 0x4);

	/* maximux page, start to move packet */
	/* SubItem 1 set max page=1, SubItem 2 set max page=2 */
	if (tDvtCmd->ucSubItemNo == DMASHDL_DVT_SUBITEM_2)
		kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR, 0x2);
	else
		kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR, 0x1);

	DMASHDL_DVT_RESET(prAdapter);
	DMASHDL_DVT_SET_SUBITEM(prAdapter, tDvtCmd->ucSubItemNo);
	DMASHDL_DVT_SET_ITEM(prAdapter, DMASHDL_DVT_ITEM_1);
	/* start to send ping packets to each group */

}

/*
* This routine is reset DMASHDL setting
*/
void dmashdl_dvt_reset_default(
	struct GLUE_INFO *prGlueInfo)
{
	uint32_t value;

	/* reset all setting and count */
	HIF_RESET_SW_LOGIC(prGlueInfo);

	/* mapping all queue to Group 0~4 */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING0_ADDR,
		0x44443210);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING1_ADDR,
		0x44444444);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING2_ADDR,
		0x44444444);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_QUEUE_MAPPING3_ADDR,
		0x44444444);

	/* set quota, max quota 0xfff, min quota 0x1*/
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR,
		0x0fff0001);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP1_CONTROL_ADDR,
		0x0fff0001);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP2_CONTROL_ADDR,
		0x0fff0001);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP3_CONTROL_ADDR,
		0x0fff0001);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_GROUP4_CONTROL_ADDR,
		0x0fff0001);

	/* enable refill */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_REFILL_CONTROL_ADDR,
		0xffe00000);

	/* group priority */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_HIF_SCHEDULER_SETTING0_ADDR,
		0x76540123);
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_HIF_SCHEDULER_SETTING1_ADDR,
		0xfedcba98);

	/* maximux page */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR,
		0x1);

	/* As default */
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_PAGE_SETTING_ADDR,
		0x3f1000);

	/* enable WA CPU mode */
	kalDevRegRead(prGlueInfo,
		WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
		&value);
	value = value |
		WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_CR_WACPU_MODE_EN_MASK;
	kalDevRegWrite(prGlueInfo,
		WF_HIF_DMASHDL_TOP_CONTROL_SIGNAL_ADDR,
		value);

	/* init quota */
	HIF_ADD_RSV_CNT(prGlueInfo, 0, 0x13);
}

/*
* This routine is used to check result.
*/
int dmashdl_dvt_check_pass(
	struct GLUE_INFO *prGlueInfo)
{
	uint32_t addr, free, status[16];
	uint32_t rsvcnt, rsvcnt2, srccnt, freecnt, ffacnt;
	uint8_t ucItemNo, ucSubItemNo;
	uint8_t i = 0;
	uint8_t result = 1;
	struct ADAPTER *pAd = NULL;

	pAd = prGlueInfo->prAdapter;

	/* get DVT item */
	ucItemNo = DMASHDL_DVT_GET_ITEM(pAd);
	ucSubItemNo = DMASHDL_DVT_GET_SUBITEM(pAd);

	/* get free page & FFA page */
	kalDevRegRead(prGlueInfo, WF_HIF_DMASHDL_TOP_STATUS_RD_ADDR, &free);

	/* fetch status of group0 ~ 15 to array */
	for (i = 0; i < ARRAY_SIZE(status); i++) {
		addr = WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_ADDR + i*4;
		kalDevRegRead(prGlueInfo, addr, &status[i]);
	}

	/* check different CR for different DVT item */
	switch (ucItemNo) {
	case DMASHDL_DVT_ITEM_1:
		for (i = 0; i < 5; i++) {
			rsvcnt = (status[i] &
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_SHFT;
			srccnt = (status[i] &
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_SRC_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_SRC_CNT_SHFT;

			/* Item 1-1 */
			/* check Group0~5 RSV count == 0 && SRC count == 4 */
			if (ucSubItemNo == DMASHDL_DVT_SUBITEM_1) {
				if (rsvcnt != 0 || srccnt != 4) {
					result = 0;
					break;
				}
			} else if (ucSubItemNo == DMASHDL_DVT_SUBITEM_2) {
			/* Item 1-2 */
			/* check Group0~5 rsv_cnt == 1 && src_cnt == 3 */
				if (rsvcnt != 1 || srccnt != 3) {
					result = 0;
					break;
				}
			}
		}

		break;
	case DMASHDL_DVT_ITEM_2:
		/* DVT Item 2 */
		/* check Group1(BE) RSV count == 2 & SRC count == 0 */
		/* check Free page == 0x13) & FFA == 0x9 */
		rsvcnt = (status[1] &
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_SHFT;
		srccnt = (status[1] &
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_SRC_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_SRC_CNT_SHFT;
		freecnt = (free &
			WF_HIF_DMASHDL_TOP_STATUS_RD_FREE_PAGE_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_FREE_PAGE_CNT_SHFT;
		ffacnt = (free &
			WF_HIF_DMASHDL_TOP_STATUS_RD_FFA_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_FFA_CNT_SHFT;
		if (rsvcnt != 2 || srccnt != 0 ||
			freecnt != 0x13 || ffacnt != 0x9)
			result = 0;

		break;
	case DMASHDL_DVT_ITEM_3:
		DBGLOG(REQ, INFO,
			"Check packet's queue id in sequence on WA\n");
		break;
	case DMASHDL_DVT_ITEM_4:
		DBGLOG(REQ, INFO,
			"Check packet's queue id in sequence on WA\n");
		break;
	case DMASHDL_DVT_ITEM_5:
		/* Item 5-1 */
		/* check Group1 rsv_cnt == 5, src_cnt == 5 */
		if (ucSubItemNo == DMASHDL_DVT_SUBITEM_1) {
			rsvcnt = (status[1] &
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_SHFT;
			srccnt = (status[1] &
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_SRC_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_SRC_CNT_SHFT;

			if (rsvcnt != 5 || srccnt != 5)
				result = 0;
		} else if (ucSubItemNo == DMASHDL_DVT_SUBITEM_2) {
			/* Item 5-2 */
			/* check Group0 rsv_cnt > Group1 rsv_cnt */
			rsvcnt = (status[0] &
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_SHFT;
			rsvcnt2 = (status[1] &
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_SHFT;

			if (rsvcnt < rsvcnt2) {
			/* failed when Group0's rescnt is less than Group1's */
				result = 0;
			}
		}

		break;
	case DMASHDL_DVT_ITEM_6:
		/* check Group0 ~ Group4 */
		/* group 0: rsv_cnt == 2, src_cnt == 0 */
		/* group 1: rsv_cnt == 0, src_cnt == 4 */
		/* group 2: rsv_cnt == 2, src_cnt == 2 */
		/* group 3: rsv_cnt == 2, src_cnt == 2 */
		/* group 4: rsv_cnt == 2, src_cnt == 0 */
		for (i = 0; i < 5; i++) {
			rsvcnt = (status[i] &
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_SHFT;
			srccnt = (status[i] &
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_SRC_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_SRC_CNT_SHFT;

			if (i == 0) {
				if (rsvcnt != 2 || srccnt != 0) {
					result = 0;
					break;
				}
			} else if (i == 1) {
				if (rsvcnt != 0 || srccnt != 4) {
					result = 0;
					break;
				}
			} else if (i == 2) {
				if (rsvcnt != 2 || srccnt != 2) {
					result = 0;
					break;
				}
			} else if (i == 3) {
				if (rsvcnt != 2 || srccnt != 2) {
					result = 0;
					break;
				}
			} else if (i == 4) {
				if (rsvcnt != 2 || srccnt != 0) {
					result = 0;
					break;
				}
			}
		}

		break;
	default:
		DBGLOG(REQ, INFO, "[DMASHDL] no support this test item\n");
	}

	return result;
}

/*
* This routine is used to end of DMASHDL DVT and check result.
* iwpriv wlan0 driver "DMASHDL_DVT_ITEM 0"
* echo "DVT PASS" if result is passed
*/
int dmashdl_dvt_result(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen,
	struct DMASHDL_DVT_CMD_T *tDvtCmd)
{
	struct ADAPTER *prAdapter = NULL;
#if (CFG_SUPPORT_CONNAC2X == 1)
	struct mt66xx_chip_info *prChipInfo;
#endif
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t idx = 0;
	uint8_t dvt_item;
	uint8_t *dvt_ping_nums;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	if (dmashdl_dvt_check_pass(prGlueInfo) == 1)
		DBGLOG(REQ, INFO, "DVT PASS\n");
	priv_driver_show_dmashdl_allcr(prNetDev, pcCommand, i4TotalLen);

#if (CFG_SUPPORT_CONNAC2X == 1)
	prChipInfo = prAdapter->chip_info;
	/* inform WACPU, this is DVT case */
	if (prChipInfo->is_support_wacpu)
		CmdExtDmaShdlDvt2WA(prAdapter,
			tDvtCmd->ucItemNo,
			tDvtCmd->ucSubItemNo);
#endif /* CFG_SUPPORT_CONNAC2X */

	if (prAdapter->auto_dvt) {
		dvt_item = prAdapter->auto_dvt->dmashdl.dvt_item;
		if (dvt_item == DMASHDL_DVT_ITEM_1
			|| dvt_item == DMASHDL_DVT_ITEM_2
			|| dvt_item == DMASHDL_DVT_ITEM_3
			|| dvt_item == DMASHDL_DVT_ITEM_4) {
			dvt_ping_nums =
			&(prAdapter->auto_dvt->dmashdl.dvt_ping_nums[0]);
			for (idx = 0; idx < 32; idx++)
				DBGLOG(REQ, INFO,
					"Ping nums %u\n", dvt_ping_nums[idx]);
		}
	}
	/* Reset DMASHDL DVT structure after result got */
	DMASHDL_DVT_RESET(prAdapter);
	/* Reset DMASHDL setting to default */
	dmashdl_dvt_reset_default(prGlueInfo);

	return i4BytesWritten;
}

int8_t cmd_atoi(uint8_t ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch - 87;
	else if (ch >= 'A' && ch <= 'F')
		return ch - 55;
	else if (ch >= '0' && ch <= '9')
		return ch - 48;

	return 0;
}

/*
* This routine is used to run DMASHDL DVT items.
* iwpriv wlan0 driver "DMASHDL_DVT_ITEM item subitem"
* For example, run item 1-2:
* iwpriv wlan0 driver "DMASHDL_DVT_ITEM 1 2"
* ping 10.10.10.1 -c 25 (Do some test)
* iwpriv wlan0 driver "DMASHDL_DVT_ITEM 0"
* PS. Item 0 is stop DVT and then check result
*/
int priv_driver_dmashdl_dvt_item(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct ADAPTER *prAdapter = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	struct DMASHDL_DVT_CMD_T tDvtCmd;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	tDvtCmd.ucItemNo = cmd_atoi(apcArgv[1][0]);
	tDvtCmd.ucArgNo = i4Argc - 2;
	if (tDvtCmd.ucArgNo)
		tDvtCmd.ucSubItemNo = cmd_atoi(apcArgv[2][0]);
	else
		tDvtCmd.ucSubItemNo = 0;

	DBGLOG(REQ, INFO, "[Item Num]=%u\n", tDvtCmd.ucItemNo);

	switch (tDvtCmd.ucItemNo) {
	case DMASHDL_DVT_RESULT:
		dmashdl_dvt_result(prNetDev, pcCommand, i4TotalLen, &tDvtCmd);
		break;
	case DMASHDL_DVT_ITEM_1:
		dmashdl_dvt_item_1(prGlueInfo, &tDvtCmd);
		break;
	case DMASHDL_DVT_ITEM_2:
		dmashdl_dvt_item_2(prGlueInfo, &tDvtCmd);
		break;
	case DMASHDL_DVT_ITEM_3:
		dmashdl_dvt_item_3(prGlueInfo, &tDvtCmd);
		break;
	case DMASHDL_DVT_ITEM_4:
		dmashdl_dvt_item_4(prGlueInfo, &tDvtCmd);
		break;
	case DMASHDL_DVT_ITEM_5:
		dmashdl_dvt_item_5(prGlueInfo, &tDvtCmd);
		break;
	case DMASHDL_DVT_ITEM_6:
		dmashdl_dvt_item_6(prGlueInfo, &tDvtCmd);
		break;
	default:
		DBGLOG(REQ, INFO, "[DMASHDL] no support this test item\n");
	}
	return i4BytesWritten;
}

/*
* This routine is used to dump related CRs about DMASHDL.
* iwpriv wlan0 driver "DMASHDL_DUMP_MEM"
*/
int priv_driver_show_dmashdl_allcr(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t addr, value;
	int32_t i4BytesWritten = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(prNetDev));

	for (addr = WF_HIF_DMASHDL_TOP_WACPU_REFILL_ADDR;
			addr <= WF_HIF_DMASHDL_TOP_GROUP15_CONTROL_ADDR;
			addr += 4) {
		kalDevRegRead(prGlueInfo, addr, &value);
		DBGLOG(REQ, INFO,
			"[DMASHDL] Addr[0x%08X], value=0x%08X\n", addr, value);
	}

	DBGLOG(REQ, INFO, "[DMASHDL] Queue Mapping\n");
	for (addr = WF_HIF_DMASHDL_TOP_QUEUE_MAPPING0_ADDR;
			addr <= WF_HIF_DMASHDL_TOP_QUEUE_MAPPING3_ADDR;
			addr += 4) {
		kalDevRegRead(prGlueInfo, addr, &value);
		DBGLOG(REQ, INFO,
			"[DMASHDL] Addr[0x%08X], value=0x%08X\n", addr, value);
	}

	kalDevRegRead(prGlueInfo, WF_HIF_DMASHDL_TOP_STATUS_RD_ADDR, &value);
	DBGLOG(REQ, INFO,
		"[DMASHDL] Status RD[0x%08X] value = 0x%08X\n",
		WF_HIF_DMASHDL_TOP_STATUS_RD_ADDR, value);
	DBGLOG(REQ, INFO, "[DMASHDL] Status RD GP\n");
	for (addr = WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_ADDR;
			addr <= WF_HIF_DMASHDL_TOP_STATUS_RD_GP15_ADDR;
			addr += 4) {
		kalDevRegRead(prGlueInfo, addr, &value);
		DBGLOG(REQ, INFO,
			"[DMASHDL] Addr[0x%08X], value=0x%08X\n", addr, value);
	}
	DBGLOG(REQ, INFO, "[DMASHDL] Status RD GP PKT cnt\n");
	for (addr = WF_HIF_DMASHDL_TOP_RD_GROUP_PKT_CNT0_ADDR;
			addr <= WF_HIF_DMASHDL_TOP_RD_GROUP_PKT_CNT7_ADDR;
			addr += 4) {
		kalDevRegRead(prGlueInfo, addr, &value);
		DBGLOG(REQ, INFO,
			"[DMASHDL] Addr[0x%08X], value=0x%08X\n", addr, value);
	}

	return i4BytesWritten;
}
#endif
