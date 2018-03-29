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

#include "stp_dbg.h"
#include "stp_dbg_combo.h"

static _osal_inline_ INT32 stp_dbg_combo_put_dump_to_aee(VOID);
static _osal_inline_ INT32 stp_dbg_combo_put_dump_to_nl(VOID);

static PUINT8 combo_task_str[COMBO_TASK_ID_INDX_MAX][COMBO_GEN3_TASK_ID_MAX] = {
	{"Task_WMT",
	"Task_BT",
	"Task_Wifi",
	"Task_Tst",
	"Task_FM",
	"Task_Idle",
	"Task_DrvStp",
	"Task_DrvSdio",
	"Task_NatBt"},
	{"Task_WMT",
	"Task_BT",
	"Task_Wifi",
	"Task_Tst",
	"Task_FM",
	"Task_GPS",
	"Task_FLP",
	"Task_BAL",
	"Task_Idle",
	"Task_DrvStp",
	"Task_DrvSdio",
	"Task_NatBt"},
};

static _osal_inline_ INT32 stp_dbg_combo_put_dump_to_aee(VOID)
{
	static UINT8 buf[2048];
	static UINT8 tmp[2048];

	UINT32 buf_len;
	STP_PACKET_T *pkt;
	STP_DBG_HDR_T *hdr;
	INT32 remain = 0;
	INT32 retry = 0;
	INT32 ret = 0;

	STP_DBG_INFO_FUNC("Enter..\n");

	do {
		remain = stp_dbg_dmp_out_ex(&buf[0], &buf_len);
		if (buf_len > 0) {
			pkt = (STP_PACKET_T *) buf;
			hdr = &pkt->hdr;
			if (hdr->dbg_type == STP_DBG_FW_DMP) {
				osal_memcpy(&tmp[0], pkt->raw, pkt->hdr.len);

				if (pkt->hdr.len <= 1500) {
					tmp[pkt->hdr.len] = '\n';
					tmp[pkt->hdr.len + 1] = '\0';

					ret = stp_dbg_aee_send(tmp, pkt->hdr.len, 0);
				} else {
					STP_DBG_INFO_FUNC("dump entry length is over long\n");
					osal_bug_on(0);
				}
				retry = 0;
			}
			retry = 0;
		} else {
			retry++;
			osal_sleep_ms(20);
		}
	} while ((remain > 0) || (retry < 10));

	STP_DBG_INFO_FUNC("Exit..\n");

	return ret;
}

static _osal_inline_ INT32 stp_dbg_combo_put_dump_to_nl(VOID)
{
#define NUM_FETCH_ENTRY 8

	static UINT8 buf[2048];
	static UINT8 tmp[2048];

	UINT32 buf_len;
	STP_PACKET_T *pkt;
	STP_DBG_HDR_T *hdr;
	INT32 remain = 0;
	INT32 index = 0;
	INT32 retry = 0;
	INT32 ret = 0;
	INT32 len;

	STP_DBG_INFO_FUNC("Enter..\n");

	index = 0;
	tmp[index++] = '[';
	tmp[index++] = 'M';
	tmp[index++] = ']';

	do {
		index = 3;
		remain = stp_dbg_dmp_out_ex(&buf[0], &buf_len);
		if (buf_len > 0) {
			pkt = (STP_PACKET_T *) buf;
			hdr = &pkt->hdr;
			len = pkt->hdr.len;
			osal_memcpy(&tmp[index], &len, 2);
			index += 2;
			if (hdr->dbg_type == STP_DBG_FW_DMP) {
				osal_memcpy(&tmp[index], pkt->raw, pkt->hdr.len);

				if (pkt->hdr.len <= 1500) {
					tmp[index + pkt->hdr.len] = '\n';
					tmp[index + pkt->hdr.len + 1] = '\0';

					/* pr_warn("\n%s\n+++\n", tmp); */
					ret = stp_dbg_dump_send_retry_handler((PINT8)&tmp, len);
					if (ret)
						break;

					/* schedule(); */
				} else {
					STP_DBG_INFO_FUNC("dump entry length is over long\n");
					osal_bug_on(0);
				}
				retry = 0;
			}
		} else {
			retry++;
			osal_sleep_ms(100);
		}
	} while ((remain > 0) || (retry < 2));

	STP_DBG_INFO_FUNC("Exit..\n");

	return ret;
}

INT32 stp_dbg_combo_core_dump(INT32 dump_sink)
{
	INT32 ret = 0;

	switch (dump_sink) {
	case 0:
		STP_DBG_INFO_FUNC("coredump is disabled!\n");
		break;
	case 1:
		ret = stp_dbg_combo_put_dump_to_aee();
		break;
	case 2:
		ret = stp_dbg_combo_put_dump_to_nl();
		break;
	default:
		ret = -1;
		STP_DBG_ERR_FUNC("unknown sink %d\n", dump_sink);
	}

	return ret;
}

PUINT8 stp_dbg_combo_id_to_task(UINT32 id)
{
	UINT32 chip_id = mtk_wcn_wmt_chipid_query();
	UINT32 task_id_indx = COMBO_TASK_ID_GEN2;
	INT32 task_id_flag = 0;

	switch (chip_id) {
	case 0x6632:
			task_id_indx = COMBO_TASK_ID_GEN3;
			if (id >= COMBO_GEN3_TASK_ID_MAX)
				task_id_flag = COMBO_GEN3_TASK_ID_MAX;
			break;
	default:
			task_id_indx = COMBO_TASK_ID_GEN2;
			if (id >= COMBO_GEN2_TASK_ID_MAX)
				task_id_flag = COMBO_GEN2_TASK_ID_MAX;
			break;
	}

	if (task_id_flag) {
			STP_DBG_ERR_FUNC("task id(%d) overflow(%d)\n", id, task_id_flag);
			return NULL;
	} else
			return combo_task_str[task_id_indx][id];
}
