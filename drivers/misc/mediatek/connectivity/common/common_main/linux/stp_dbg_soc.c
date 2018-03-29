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
#include "stp_dbg_soc.h"
#include "btm_core.h"
#include "stp_core.h"

#define STP_DBG_PAGED_DUMP_BUFFER_SIZE (32*1024*sizeof(char))
#define STP_DBG_PAGED_TRACE_SIZE (2048*sizeof(char))
#define SUB_PKT_SIZE 1024
#define SUB_PKT_HEADER 5

ENUM_STP_FW_ISSUE_TYPE issue_type;
UINT8 g_paged_trace_buffer[STP_DBG_PAGED_TRACE_SIZE] = { 0 };
UINT32 g_paged_trace_len = 0;
UINT8 g_paged_dump_buffer[STP_DBG_PAGED_DUMP_BUFFER_SIZE] = { 0 };
UINT32 g_paged_dump_len = 0;

static PUINT8 soc_task_str[SOC_TASK_ID_INDX_MAX][SOC_GEN3_TASK_ID_MAX] = {
	{"Task_WMT",
	"Task_BT",
	"Task_Wifi",
	"Task_Tst",
	"Task_FM",
	"Task_Idle",
	"Task_DrvStp",
	"Task_DrvBtif",
	"Task_NatBt"},
	{"Task_WMT",
	"Task_BT",
	"Task_Wifi",
	"Task_Tst",
	"Task_FM",
	"Task_GPS",
	"Task_FLP",
	"Task_NULL",
	"Task_Idle",
	"Task_DrvStp",
	"Task_DrvBtif",
	"Task_NatBt",
	"Task_DrvWifi"},
};

static _osal_inline_ INT32 stp_dbg_soc_paged_dump(INT32 dump_sink);
static _osal_inline_ INT32 stp_dbg_soc_paged_trace(VOID);
static _osal_inline_ INT32 stp_dbg_soc_put_emi_dump_to_nl(PUINT8 data_buf, INT32 dump_len);
static _osal_inline_ VOID stp_dbg_soc_emi_dump_buffer(UINT8 *buffer, UINT32 len);

INT32 __weak wmt_plat_get_dump_info(UINT32 offset)
{
	STP_DBG_ERR_FUNC("wmt_plat_get_dump_info is not define!!!\n");

	return 0;
}

INT32 __weak wmt_plat_set_host_dump_state(ENUM_HOST_DUMP_STATE state)
{
	STP_DBG_ERR_FUNC("wmt_plat_set_host_dump_state is not define!!!\n");

	return 0;
}

INT32 __weak wmt_plat_update_host_sync_num(VOID)
{
	STP_DBG_ERR_FUNC("wmt_plat_update_host_sync_num is not define!!!\n");

	return 0;
}

static _osal_inline_ VOID stp_dbg_soc_emi_dump_buffer(UINT8 *buffer, UINT32 len)
{
	UINT32 i = 0;

	if (len > 16)
		len = 16;
	for (i = 0; i < len; i++) {
		if (i % 16 == 0 && i != 0)
			pr_cont("\n    ");

		if (buffer[i] == ']' || buffer[i] == '[' || buffer[i] == ',')
			pr_cont("%c", buffer[i]);
		else
			pr_cont("0x%02x ", buffer[i]);
	}
}

static _osal_inline_ INT32 stp_dbg_soc_put_emi_dump_to_nl(PUINT8 data_buf, INT32 dump_len)
{
	static UINT8  tmp[SUB_PKT_SIZE + SUB_PKT_HEADER];
	INT32 remain = dump_len, index = 0;
	INT32 ret = 0;
	INT32 len;
	INT32 offset = 0;

	STP_DBG_INFO_FUNC("Enter..\n");

	if (dump_len > 0) {
		index = 0;
		tmp[index++] = '[';
		tmp[index++] = 'M';
		tmp[index++] = ']';

		do {
			index = 3;
			if (remain >= SUB_PKT_SIZE)
				len = SUB_PKT_SIZE;
			else
				len = remain;
			remain -= len;

			osal_memcpy(&tmp[index], &len, 2);
			index += 2;
			osal_memcpy(&tmp[index], data_buf + offset, len);
			offset += len;
			STP_DBG_DBG_FUNC("send %d remain %d\n", len, remain);

			ret = stp_dbg_dump_send_retry_handler((PINT8)&tmp, len);
			if (ret)
				break;

			/* schedule(); */
		} while (remain > 0);
	} else
		STP_DBG_INFO_FUNC("dump entry length is 0\n");
	STP_DBG_INFO_FUNC("Exit..\n");

	return ret;
}

static _osal_inline_ INT32 stp_dbg_soc_paged_dump(INT32 dump_sink)
{
	INT32 ret = 0;
	UINT32 counter = 0;
	UINT32 dump_num = 0;
	UINT32 packet_num = STP_PAGED_DUMP_TIME_LIMIT/100;
	UINT32 page_counter = 0;
	UINT32 loop_cnt1 = 0;
	UINT32 loop_cnt2 = 0;
	ENUM_HOST_DUMP_STATE host_state;
	ENUM_CHIP_DUMP_STATE chip_state;
	UINT32 dump_phy_addr = 0;
	PUINT8 dump_vir_addr = NULL;
	UINT32 dump_len = 0;
	UINT32 isEnd = 0;
	P_CONSYS_EMI_ADDR_INFO p_ecsi;

	p_ecsi = wmt_plat_get_emi_phy_add();
	osal_assert(p_ecsi);

	issue_type = STP_FW_ASSERT_ISSUE;
	if (chip_reset_only) {
		chip_reset_only = 0;
		STP_DBG_WARN_FUNC("is chip reset only\n");
		ret = -1;
		return ret;
	}

	/*packet number depend on dump_num get from register:0xf0080044 ,support jade*/
	stp_dbg_core_dump_deinit_gcoredump();
	dump_num = wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_chip_page_dump_num);
	if (dump_num != 0) {
		packet_num = dump_num;
		STP_DBG_WARN_FUNC("get consys dump num packet_num(%d)\n", packet_num);
	} else {
		STP_DBG_ERR_FUNC("can not get consys dump num and default num is 35\n");
	}
	ret = stp_dbg_core_dump_init_gcoredump(packet_num, STP_CORE_DUMP_TIMEOUT);
	if (ret) {
		STP_DBG_ERR_FUNC("core dump init fail\n");
		return ret;
	}

	wmt_plat_set_host_dump_state(STP_HOST_DUMP_NOT_START);
	page_counter = 0;
	do {
		loop_cnt1 = 0;
		loop_cnt2 = 0;
		dump_phy_addr = 0;
		dump_vir_addr = NULL;
		dump_len = 0;
		isEnd = 0;

		host_state = (ENUM_HOST_DUMP_STATE)wmt_plat_get_dump_info(
				p_ecsi->p_ecso->emi_apmem_ctrl_host_sync_state);
		if (STP_HOST_DUMP_NOT_START == host_state) {
			counter++;
			STP_DBG_INFO_FUNC("counter(%d)\n", counter);
			osal_sleep_ms(100);
		} else {
			counter = 0;
		}
		while (1) {
			chip_state = (ENUM_CHIP_DUMP_STATE)wmt_plat_get_dump_info(
					p_ecsi->p_ecso->emi_apmem_ctrl_chip_sync_state);
			if (STP_CHIP_DUMP_PUT_DONE == chip_state) {
				STP_DBG_INFO_FUNC("chip put done\n");
				break;
			}
			STP_DBG_DBG_FUNC("waiting chip put done\n");
			STP_DBG_INFO_FUNC("chip_state: %d\n", chip_state);
			loop_cnt1++;
			osal_sleep_ms(5);

			if (loop_cnt1 > 10)
				goto paged_dump_end;
		}

		wmt_plat_set_host_dump_state(STP_HOST_DUMP_GET);

		dump_phy_addr = wmt_plat_get_dump_info(
				p_ecsi->p_ecso->emi_apmem_ctrl_chip_sync_addr);

		if (!dump_phy_addr) {
			STP_DBG_ERR_FUNC("get paged dump phy address fail\n");
			ret = -1;
			break;
		}

		dump_vir_addr = wmt_plat_get_emi_virt_add(dump_phy_addr - p_ecsi->emi_phy_addr);
		if (!dump_vir_addr) {
			STP_DBG_ERR_FUNC("get paged dump phy address fail\n");
			ret = -2;
			break;
		}
		dump_len = wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_chip_sync_len);
		STP_DBG_DBG_FUNC("dump_phy_ddr(%08x),dump_vir_add(0x%p),dump_len(%d)\n",
				dump_phy_addr, dump_vir_addr, dump_len);

		/*move dump info according to dump_addr & dump_len */
		osal_memcpy(&g_paged_dump_buffer[0], dump_vir_addr, dump_len);
		/*stp_dbg_soc_emi_dump_buffer(&g_paged_dump_buffer[0], dump_len);*/

		if (0 == page_counter) {
			/* do fw assert infor paser in first paged dump */
			if (1 == stp_dbg_get_host_trigger_assert())
				issue_type = STP_HOST_TRIGGER_FW_ASSERT;

			ret = stp_dbg_set_fw_info(&g_paged_dump_buffer[0], 512, issue_type);
			if (ret) {
				STP_DBG_ERR_FUNC("set fw issue infor fail(%d),maybe fw warm reset...\n",
						ret);
				stp_dbg_set_fw_info("Fw Warm reset", osal_strlen("Fw Warm reset"),
						STP_FW_WARM_RST_ISSUE);
			}
		}

		if (dump_len <= 32 * 1024) {
			pr_err("coredump mode: %d!\n", dump_sink);
			switch (dump_sink) {
			case 0:
				STP_DBG_INFO_FUNC("coredump is disabled!\n");
				return 0;
			case 1:
				ret = stp_dbg_aee_send(&g_paged_dump_buffer[0], dump_len, 0);
				if (ret == 0)
					STP_DBG_INFO_FUNC("aee send ok!\n");
				else if (ret == 1)
					STP_DBG_INFO_FUNC("aee send fisish!\n");
				else
					STP_DBG_ERR_FUNC("aee send error!\n");
				break;
			case 2:
				ret = stp_dbg_soc_put_emi_dump_to_nl(&g_paged_dump_buffer[0], dump_len);
				if (ret == 0)
					STP_DBG_INFO_FUNC("dump send ok!\n");
				else if (ret == 1)
					STP_DBG_INFO_FUNC("dump send timeout!\n");
				else
					STP_DBG_ERR_FUNC("dump send error!\n");
				break;
			default:
				STP_DBG_ERR_FUNC("unknown sink %d\n", dump_sink);
				return -1;
			}
		} else
			STP_DBG_ERR_FUNC("dump len is over than 32K(%d)\n", dump_len);

		g_paged_dump_len += dump_len;
		STP_DBG_INFO_FUNC("dump len update(%d)\n", g_paged_dump_len);
		wmt_plat_update_host_sync_num();
		wmt_plat_set_host_dump_state(STP_HOST_DUMP_GET_DONE);

		STP_DBG_INFO_FUNC("host sync num(%d),chip sync num(%d)\n",
				wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_host_sync_num),
				wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_chip_sync_num));
		page_counter++;
		STP_DBG_INFO_FUNC("\n\n++ paged dump counter(%d) ++\n\n", page_counter);

		while (1) {
			chip_state = (ENUM_CHIP_DUMP_STATE)wmt_plat_get_dump_info(
					p_ecsi->p_ecso->emi_apmem_ctrl_chip_sync_state);
			if (STP_CHIP_DUMP_END == chip_state) {
				STP_DBG_INFO_FUNC("chip put end\n");
				wmt_plat_set_host_dump_state(STP_HOST_DUMP_END);
				break;
			}
			STP_DBG_INFO_FUNC("waiting chip put end, chip_state: %d\n", chip_state);
			loop_cnt2++;
			osal_sleep_ms(10);

			if (loop_cnt2 > 10)
				goto paged_dump_end;
		}

paged_dump_end:
		wmt_plat_set_host_dump_state(STP_HOST_DUMP_NOT_START);

		/* coredump mode 2 return timeout exit*/
		if (2 == dump_sink && 1 == ret)
			break;

		if (counter > packet_num) {
			isEnd = wmt_plat_get_dump_info(
					p_ecsi->p_ecso->emi_apmem_ctrl_chip_paded_dump_end);

			if (isEnd) {
				STP_DBG_INFO_FUNC("paged dump end\n");

				STP_DBG_INFO_FUNC("\n\n paged dump print  ++\n\n");
				stp_dbg_soc_emi_dump_buffer(&g_paged_dump_buffer[0], g_paged_dump_len);
				STP_DBG_INFO_FUNC("\n\n paged dump print  --\n\n");
				STP_DBG_INFO_FUNC("\n\n paged dump size = %d, paged dump page number = %d\n\n",
						g_paged_dump_len, page_counter);
				counter = 0;
				ret = 0;
			} else {
				STP_DBG_ERR_FUNC("paged dump fail\n");
				wmt_plat_set_host_dump_state(STP_HOST_DUMP_NOT_START);
				stp_dbg_poll_cpupcr(5, 5, 0);
				stp_dbg_poll_dmaregs(5, 1);
				counter = 0;
				ret = -1;
			}
			break;
		}
	} while (1);

	return ret;
}

static _osal_inline_ INT32 stp_dbg_soc_paged_trace(VOID)
{
	INT32 ret = 0;
	UINT32 ctrl_val = 0;
	UINT32 loop_cnt1 = 0;
	UINT32 buffer_start = 0;
	UINT32 buffer_idx = 0;
	PUINT8 dump_vir_addr = NULL;
	P_CONSYS_EMI_ADDR_INFO p_ecsi;
	INT32 i = 0;
	INT32 dump_len = 0;
	UINT8 str[70];
	PUINT8 p_str;

	p_ecsi = wmt_plat_get_emi_phy_add();
	do {
		ctrl_val = 0;
		loop_cnt1 = 0;
		buffer_start = 0;
		buffer_idx = 0;
		dump_vir_addr = NULL;

		while (loop_cnt1 < 10) {
			ctrl_val = wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_state);
			if (0x8 == ctrl_val)
				break;
			osal_sleep_ms(10);
			loop_cnt1++;
		}
		if (loop_cnt1 >= 10) {
			STP_DBG_ERR_FUNC("polling CTRL STATE fail\n");
			ret = -1;
			break;
		}

		buffer_start = wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_chip_print_buff_start);
		buffer_idx = wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_chip_print_buff_idx);
		g_paged_trace_len = buffer_idx;
		STP_DBG_INFO_FUNC("paged trace buffer addr(%08x),buffer_len(%d)\n", buffer_start,
				buffer_idx);
		dump_vir_addr = wmt_plat_get_emi_virt_add(buffer_start - p_ecsi->emi_phy_addr);
		if (!dump_vir_addr) {
			STP_DBG_ERR_FUNC("get vir dump address fail\n");
			ret = -2;
			break;
		}
		osal_memcpy(&g_paged_trace_buffer[0], dump_vir_addr,
				buffer_idx < STP_DBG_PAGED_TRACE_SIZE ? buffer_idx : STP_DBG_PAGED_TRACE_SIZE);
		/*moving paged trace according to buffer_start & buffer_len */
		do {
			dump_len =
				buffer_idx < STP_DBG_PAGED_TRACE_SIZE ? buffer_idx : STP_DBG_PAGED_TRACE_SIZE;
			pr_warn("\n\n -- paged trace ascii output --\n\n");
			p_str = &str[0];
			for (i = 0; i < dump_len; i++) {
				sprintf(p_str, "%c ", g_paged_trace_buffer[i]);
				p_str++;
				if (0 == (i % 64)) {
					*p_str++ = '\n';
					*p_str = '\0';
					pr_debug("%s", str);
					p_str = &str[0];
				}
			}
			if (dump_len % 64) {
				*p_str++ = '\n';
				*p_str = '\0';
				pr_debug("%s", str);
			}
		} while (0);
		/*move parser fw assert infor to paged dump in the one paged dump */
		/* ret = stp_dbg_set_fw_info(&g_paged_trace_buffer[0], g_paged_trace_len, issue_type); */
		ret = 0;
	} while (0);
	mtk_wcn_stp_ctx_restore();

	return ret;
}

INT32 stp_dbg_soc_core_dump(INT32 dump_sink)
{
	INT32 ret = 0;

	ret = stp_dbg_soc_paged_dump(dump_sink);
	if (ret)
		STP_DBG_ERR_FUNC("stp_dbg_soc_paged_dump fail: %d!\n", ret);

	ret = stp_dbg_soc_paged_trace();
	if (ret)
		STP_DBG_ERR_FUNC("stp_dbg_soc_paged_trace fail: %d!\n", ret);

	return ret;
}

PUINT8 stp_dbg_soc_id_to_task(UINT32 id)
{
	UINT32 chip_id = wmt_plat_get_soc_chipid();
	UINT32 task_id_indx = SOC_TASK_ID_GEN2;
	INT32 task_id_flag = 0;

	switch (chip_id) {
	case 0x6797:
		task_id_indx = SOC_TASK_ID_GEN3;
		if (id >= SOC_GEN3_TASK_ID_MAX)
			task_id_flag = SOC_GEN3_TASK_ID_MAX;
		break;
	default:
		task_id_indx = SOC_TASK_ID_GEN2;
		if (id >= SOC_GEN2_TASK_ID_MAX)
			task_id_flag = SOC_GEN2_TASK_ID_MAX;
		break;
	}

	if (task_id_flag) {
		STP_DBG_ERR_FUNC("task id(%d) overflow(%d)\n", id, task_id_flag);
		return NULL;
	} else
		return soc_task_str[task_id_indx][id];
}

UINT32 stp_dbg_soc_read_debug_crs(ENUM_CONNSYS_DEBUG_CR cr)
{
#define CONSYS_REG_READ(addr) (*((volatile UINT32 *)(addr)))
	UINT8 *consys_dbg_cr_base = NULL;

	consys_dbg_cr_base = ioremap_nocache(CONSYS_DBG_CR_BASE, 0x500);
	switch (cr) {
	case CONNSYS_CPU_CLK:
		return CONSYS_REG_READ(consys_dbg_cr_base + CONSYS_CPU_CLK_STATUS_OFFSET);
	case CONNSYS_BUS_CLK:
		return CONSYS_REG_READ(consys_dbg_cr_base + CONSYS_BUS_CLK_STATUS_OFFSET);
	case CONNSYS_DEBUG_CR1:
		return CONSYS_REG_READ(consys_dbg_cr_base + CONSYS_DBG_CR1_OFFSET);
	case CONNSYS_DEBUG_CR2:
		return CONSYS_REG_READ(consys_dbg_cr_base + CONSYS_DBG_CR2_OFFSET);
	default:
		return 0;
	}
}
