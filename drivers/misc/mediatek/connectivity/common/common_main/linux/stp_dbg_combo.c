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

static PUINT8 combo_task_str[STP_DBG_TASK_ID_MAX] = {
	"Task_WMT",
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
	"Task_NatBt",
	"Task_DrvWifi",
	"Task_GPS"
};

INT32 const combo_legacy_task_id_adapter[STP_DBG_TASK_ID_MAX] = {
	STP_DBG_TASK_WMT,
	STP_DBG_TASK_BT,
	STP_DBG_TASK_WIFI,
	STP_DBG_TASK_TST,
	STP_DBG_TASK_FM,
	STP_DBG_TASK_IDLE,
	STP_DBG_TASK_WMT,
	STP_DBG_TASK_WMT,
	STP_DBG_TASK_WMT,
	STP_DBG_TASK_DRVSTP,
	STP_DBG_TASK_BUS,
	STP_DBG_TASK_NATBT,
	STP_DBG_TASK_DRVWIFI,
	STP_DBG_TASK_DRVGPS
};

static _osal_inline_ INT32 stp_dbg_combo_put_dump_to_aee(VOID)
{
	static UINT8 buf[2048];
	static UINT8 tmp[2048];

	UINT32 buf_len;
	STP_PACKET_T *pkt = NULL;
	STP_DBG_HDR_T *hdr = NULL;
	INT32 remain = 0;
	INT32 retry = 0;
	INT32 ret = 0;

	do {
		remain = stp_dbg_dmp_out_ex(&buf[0], &buf_len);
		if (buf_len > 0) {
			pkt = (STP_PACKET_T *) buf;
			hdr = &pkt->hdr;
			if (hdr->dbg_type == STP_DBG_FW_DMP) {
				if (pkt->hdr.len <= 1500) {
					tmp[pkt->hdr.len] = '\n';
					tmp[pkt->hdr.len + 1] = '\0';
					if (pkt->hdr.len < STP_DMP_SZ)
						osal_memcpy(&tmp[0], pkt->raw, pkt->hdr.len);
					else
						osal_memcpy(&tmp[0], pkt->raw, STP_DMP_SZ);
					ret = stp_dbg_aee_send(tmp, pkt->hdr.len, 0);
				} else {
					STP_DBG_PR_INFO("dump entry length is over long\n");
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

	return ret;
}

static _osal_inline_ INT32 stp_dbg_combo_put_dump_to_nl(VOID)
{
#define NUM_FETCH_ENTRY 8

	static UINT8 buf[2048];
	static UINT8 tmp[2048];

	UINT32 buf_len;
	STP_PACKET_T *pkt = NULL;
	STP_DBG_HDR_T *hdr = NULL;
	INT32 remain = 0;
	INT32 index = 0;
	INT32 retry = 0;
	INT32 ret = 0;
	INT32 len;

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
					STP_DBG_PR_INFO("dump entry length is over long\n");
					osal_bug_on(0);
				}
				retry = 0;
			}
		} else {
			retry++;
			osal_sleep_ms(100);
		}
	} while ((remain > 0) || (retry < 2));

	return ret;
}

INT32 stp_dbg_combo_core_dump(INT32 dump_sink)
{
	INT32 ret = 0;

	switch (dump_sink) {
	case 0:
		STP_DBG_PR_INFO("coredump is disabled!\n");
		break;
	case 1:
		ret = stp_dbg_combo_put_dump_to_aee();
		break;
	case 2:
		ret = stp_dbg_combo_put_dump_to_nl();
		break;
	default:
		ret = -1;
		STP_DBG_PR_ERR("unknown sink %d\n", dump_sink);
	}

	return ret;
}

PUINT8 stp_dbg_combo_id_to_task(UINT32 id)
{
	UINT32 chip_id = mtk_wcn_wmt_chipid_query();
	UINT32 temp_id;

	if (id >= STP_DBG_TASK_ID_MAX) {
		STP_DBG_PR_ERR("task id(%d) overflow(%d)\n", id, STP_DBG_TASK_ID_MAX);
		return NULL;
	}

	switch (chip_id) {
	case 0x6632:
		temp_id = id;
		break;
	default:
		temp_id = combo_legacy_task_id_adapter[id];
		break;
	}

	return combo_task_str[temp_id];
}
