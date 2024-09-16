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

#include "osal.h"
#include "wmt_idc.h"
#include "wmt_lib.h"

#if CFG_WMT_LTE_COEX_HANDLING

MTK_WCN_WMT_IDC_INFO gWmtIdcInfo;

INT32 wmt_idc_init(VOID)
{
	INT32 iRet;

	osal_memset(&gWmtIdcInfo, 0, osal_sizeof(gWmtIdcInfo));
	gWmtIdcInfo.iit.src_mod_id = AP_MOD_WMT;
	gWmtIdcInfo.iit.dest_mod_id = MD_MOD_EL1;
	gWmtIdcInfo.iit.sap_id = 0;
	gWmtIdcInfo.ops.rx_cb = wmt_idc_msg_from_lte_handing;

	iRet = mtk_conn_md_bridge_reg(gWmtIdcInfo.iit.src_mod_id, &gWmtIdcInfo.ops);
	if (iRet) {
		WMT_ERR_FUNC("mtk_conn_md_bridge_reg fail(%d)\n", iRet);
		return -1;
	}
	/* mtk_wcn_stp_flush_rx_queue(COEX_TASK_INDX); */
	return 0;
}

INT32 wmt_idc_deinit(VOID)
{
	INT32 iRet;

	iRet = mtk_conn_md_bridge_unreg(gWmtIdcInfo.iit.src_mod_id);
	if (iRet)
		WMT_ERR_FUNC("mtk_conn_md_bridge_unreg fail(%d)\n", iRet);

	osal_memset(&gWmtIdcInfo, 0, osal_sizeof(gWmtIdcInfo));

	return 0;
}

INT32 wmt_idc_msg_from_lte_handing(conn_md_ipc_ilm_t *ilm)
{
	MTK_WCN_BOOL bRet;

	if (ilm == NULL) {
		WMT_ERR_FUNC("NULL pointer\n");
		return -1;
	}
	if (mtk_wcn_stp_is_ready()) {
		bRet = wmt_lib_handle_idc_msg(ilm);
		if (bRet == MTK_WCN_BOOL_FALSE) {
			WMT_ERR_FUNC("wmt handing idc msg fail\n");
			return -2;
		}
	} else
		WMT_INFO_FUNC("Received LTE msg,but STP is not ready,drop it!\n");

	return 0;
}

VOID wmt_idc_dump_debug_msg(PUINT8 str, PUINT8 p_buf, UINT32 buf_len)
{
	UINT32 idx = 0;

	WMT_DBG_FUNC("%s:, length:%d\n", str, buf_len);

	WMT_DBG_FUNC("ASCII output:\n");

	for (idx = 0; idx < buf_len;) {
		WMT_DBG_FUNC("%c", p_buf[idx]);
		idx++;
		if (idx % 16 == 0)
			WMT_DBG_FUNC("\n");
	}

	WMT_DBG_FUNC("HEX output:\n");

	for (idx = 0; idx < buf_len;) {
		WMT_DBG_FUNC("%02x ", p_buf[idx]);
		idx++;
		if (idx % 16 == 0)
			WMT_DBG_FUNC("\n");
	}
}

INT32 wmt_idc_msg_to_lte_handing(VOID)
{
	UINT32 readlen = 0;
	struct local_para *p_lps = NULL;
	PUINT8 p_data = NULL;
	UINT8 opcode = 0;
	UINT16 msg_len = 0;
#if	CFG_WMT_LTE_ENABLE_MSGID_MAPPING
	MTK_WCN_BOOL unknown_msgid = MTK_WCN_BOOL_FALSE;
#endif

	readlen = mtk_wcn_stp_receive_data(&gWmtIdcInfo.buffer[0], 4, COEX_TASK_INDX);
	if (readlen == 0) {
		osal_sleep_ms(5);
		readlen = mtk_wcn_stp_receive_data(&gWmtIdcInfo.buffer[0], 4, COEX_TASK_INDX);
	}

	if (readlen > 0) {
		WMT_DBG_FUNC("read data len from fw(%d)\n", readlen);
		wmt_idc_dump_debug_msg("WMT->LTE from STP buffer", &gWmtIdcInfo.buffer[0], readlen);

		while (readlen) {
			p_data = &gWmtIdcInfo.buffer[0];
			p_data += 2;	/*omit direction & opcode 2 bytes */
			osal_memcpy(&msg_len, p_data, 2);
			WMT_DBG_FUNC("current raw data len(%d) from connsys firmware\n", msg_len);

			msg_len = msg_len > LTE_IDC_BUFFER_MAX_SIZE ? LTE_IDC_BUFFER_MAX_SIZE : msg_len;
			osal_memset(&gWmtIdcInfo.buffer[0], 0, LTE_IDC_BUFFER_MAX_SIZE);
			readlen = mtk_wcn_stp_receive_data(&gWmtIdcInfo.buffer[0], msg_len, COEX_TASK_INDX);
			p_data = &gWmtIdcInfo.buffer[0];

			if (msg_len > 0)
				msg_len -= 1;	/*flag byte */
			else
				WMT_ERR_FUNC("msg_len is ERROR!");

			/*how to handle flag(msg type) need to Scott comment */
			/************************************************/

			if (*p_data == WMT_IDC_RX_OPCODE_DEBUG_MONITOR)
				/*do not need transfer to LTE */
			{
				p_data += 1;	/*flag : 1 byte */
				/*need to handle these debug message */
				wmt_idc_dump_debug_msg("WIFI DEBUG MONITOR", p_data, msg_len);
			} else
				/*need to transfer to LTE */
			{
				p_lps =
				    (struct local_para *) osal_malloc(osal_sizeof(struct local_para) +
								      osal_sizeof(UINT8) * msg_len);
				if (p_lps == NULL) {
					WMT_ERR_FUNC("allocate struct local_para memory fail\n");
					return -1;
				}

				p_lps->msg_len = msg_len + osal_sizeof(struct local_para);

				opcode = *p_data;
				WMT_DBG_FUNC("current opcode(%d) to LTE\n", opcode);

				p_data += 1;	/*flag : 1 byte */
				osal_memcpy(p_lps->data, p_data, msg_len);

				gWmtIdcInfo.iit.local_para_ptr = p_lps;

#if	CFG_WMT_LTE_ENABLE_MSGID_MAPPING
				switch (opcode) {
				case WMT_IDC_RX_OPCODE_BTWF_DEF_PARA:
					gWmtIdcInfo.iit.msg_id = IPC_MSG_ID_EL1_WIFIBT_OPER_DEFAULT_PARAM_IND;
					break;
				case WMT_IDC_RX_OPCODE_BTWF_CHAN_RAN:
					gWmtIdcInfo.iit.msg_id = IPC_MSG_ID_EL1_WIFIBT_OPER_FREQ_IND;
					break;
				case WMT_IDC_RX_OPCODE_LTE_FREQ_IDX_TABLE:
					gWmtIdcInfo.iit.msg_id = IPC_MSG_ID_EL1_WIFIBT_FREQ_IDX_TABLE_IND;
					break;
				case WMT_IDC_RX_OPCODE_BTWF_PROFILE_IND:
					gWmtIdcInfo.iit.msg_id = IPC_MSG_ID_EL1_WIFIBT_PROFILE_IND;
					break;
				case WMT_IDC_RX_OPCODE_UART_PIN_SEL:
					gWmtIdcInfo.iit.msg_id = IPC_MSG_ID_EL1_PIN_TYPE_IND;
					break;
					/* case WMT_IDC_RX_OPCODE_TDM_REQ: */
					/* gWmtIdcInfo.iit.msg_id = IPC_MSG_ID_EL1_WIFIBT_OPER_FREQ_IND; */
					/* break; */
				default:
					unknown_msgid = MTK_WCN_BOOL_TRUE;
					WMT_ERR_FUNC("unknown opcode(%d) from connsys firmware\n", opcode);
					break;
				}
				if (unknown_msgid == MTK_WCN_BOOL_FALSE) {
					/*handling flag value in wmt cmd */
					mtk_conn_md_bridge_send_msg(&gWmtIdcInfo.iit);
				}
#else
				if (opcode >= LTE_MSG_ID_OFFSET) {
					gWmtIdcInfo.iit.msg_id =
						opcode + IPC_EL1_MSG_ID_BEGIN - LTE_MSG_ID_OFFSET + 1;
					/*handling flag value in wmt cmd */
					if (gWmtIdcInfo.iit.msg_id == IPC_MSG_ID_MD_CONSYS_VERIFICATION_REQ)
						gWmtIdcInfo.iit.dest_mod_id = MD_MOD_GMMGR;
					else
						gWmtIdcInfo.iit.dest_mod_id = MD_MOD_EL1;

					mtk_conn_md_bridge_send_msg(&gWmtIdcInfo.iit);
					WMT_DBG_FUNC("CONN->LTE: (0x%x->0x%x)\n", opcode,
							gWmtIdcInfo.iit.msg_id);
				} else
					WMT_ERR_FUNC("opcode(%d)from connsys fw is out of range,drop it!\n",
							opcode);
#endif
				osal_free(p_lps);
			}

			osal_memset(&gWmtIdcInfo.buffer[0], 0, LTE_IDC_BUFFER_MAX_SIZE);
			readlen = mtk_wcn_stp_receive_data(&gWmtIdcInfo.buffer[0], 4, COEX_TASK_INDX);
			WMT_DBG_FUNC("read data len from fw(%d)\n", readlen);
		}

	} else
		WMT_ERR_FUNC("there is no coex data in stp buffer\n");

	osal_memset(&gWmtIdcInfo.buffer[0], 0, LTE_IDC_BUFFER_MAX_SIZE);

	return 0;
}

UINT32 wmt_idc_msg_to_lte_handing_for_test(PUINT8 p_buf, UINT32 len)
{
	UINT32 readlen = len;
	struct local_para *p_lps = NULL;
	PUINT8 p_data = NULL;
	UINT8 opcode = 0;
	UINT16 msg_len = 0;
	MTK_WCN_BOOL unknown_msgid = MTK_WCN_BOOL_FALSE;

	osal_memcpy(&gWmtIdcInfo.buffer[0], p_buf, len);

	if (readlen > 0) {
		WMT_DBG_FUNC("read data len from fw(%d)\n", readlen);

		while (readlen) {
			p_data = &gWmtIdcInfo.buffer[0];
			p_data += 2;	/*omit direction & opcode 2 bytes */
			osal_memcpy(&msg_len, p_data, 2);

			msg_len = msg_len > LTE_IDC_BUFFER_MAX_SIZE ? LTE_IDC_BUFFER_MAX_SIZE : msg_len;
			osal_memset(&gWmtIdcInfo.buffer[0], 0, LTE_IDC_BUFFER_MAX_SIZE);
			readlen = mtk_wcn_stp_receive_data(&gWmtIdcInfo.buffer[0], msg_len, COEX_TASK_INDX);
			p_data = &gWmtIdcInfo.buffer[0];

			if (msg_len > 0)
				msg_len -= 1;	/*flag byte */
			else
				WMT_ERR_FUNC("msg_len is ERROR!");
			WMT_DBG_FUNC("current raw data len(%d) from connsys firmware\n", msg_len);

			/*how to handle flag(msg type) need to Scott comment */
			/************************************************/

			if (*p_data == WMT_IDC_RX_OPCODE_DEBUG_MONITOR)
				/*do not need transfer to LTE */
			{
				p_data += 1;	/*flag : 1 byte */
				/*need to handle these debug message */
				wmt_idc_dump_debug_msg("WIFI DEBUG MONITOR", p_data, msg_len);
			} else {
				/*need to transfer to LTE */
				p_lps = (struct local_para *) osal_malloc(osal_sizeof(struct local_para) +
						osal_sizeof(UINT8) * msg_len);
				if (p_lps == NULL) {
					WMT_ERR_FUNC("allocate struct local_para memory fail\n");
					return -1;
				}

				p_lps->msg_len = msg_len + osal_sizeof(struct local_para);

				opcode = *p_data;
				WMT_DBG_FUNC("current opcode(%d) to LTE\n", opcode);

				p_data += 1;	/*flag : 1 byte */
				osal_memcpy(p_lps->data, p_data, msg_len);

				gWmtIdcInfo.iit.local_para_ptr = p_lps;

				switch (opcode) {
				case WMT_IDC_RX_OPCODE_BTWF_DEF_PARA:
					gWmtIdcInfo.iit.msg_id = IPC_MSG_ID_EL1_WIFIBT_OPER_DEFAULT_PARAM_IND;
					break;
				case WMT_IDC_RX_OPCODE_BTWF_CHAN_RAN:
					gWmtIdcInfo.iit.msg_id = IPC_MSG_ID_EL1_WIFIBT_OPER_FREQ_IND;
					break;
					/* case WMT_IDC_RX_OPCODE_TDM_REQ: */
					/* gWmtIdcInfo.iit.msg_id = IPC_MSG_ID_EL1_WIFIBT_OPER_FREQ_IND; */
					/* break; */
				default:
					unknown_msgid = MTK_WCN_BOOL_TRUE;
					WMT_ERR_FUNC("unknown opcode(%d) from connsys firmware\n", opcode);
				}
				if (unknown_msgid == MTK_WCN_BOOL_FALSE) {
					/*handling flag value in wmt cmd */
					mtk_conn_md_bridge_send_msg(&gWmtIdcInfo.iit);
				}
				osal_free(p_lps);
			}

			osal_memset(&gWmtIdcInfo.buffer[0], 0, LTE_IDC_BUFFER_MAX_SIZE);
			readlen = mtk_wcn_stp_receive_data(&gWmtIdcInfo.buffer[0], 4, COEX_TASK_INDX);
		}

	} else
		WMT_ERR_FUNC("there is no coex data in stp buffer\n");

	osal_memset(&gWmtIdcInfo.buffer[0], 0, LTE_IDC_BUFFER_MAX_SIZE);

	return 0;
}
#endif /* CFG_WMT_LTE_COEX_HANDLING */
