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

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#include "osal_typedef.h"
#include "wmt_dbg.h"
#include "wmt_core.h"
#include "wmt_lib.h"
#include "wmt_conf.h"
#include "psm_core.h"
#include "stp_core.h"
#include "stp_dbg.h"

#ifdef DFT_TAG
#undef DFT_TAG
#define DFT_TAG         "[WMT-DEV]"
#endif

#define WMT_DBG_PROCNAME "driver/wmt_dbg"

#define BUF_LEN_MAX 384

#if (defined(CONFIG_MTK_GMO_RAM_OPTIMIZE) && !defined(CONFIG_MT_ENG_BUILD))
#define WMT_EMI_DEBUG_BUF_SIZE (8*1024)
#else
#define WMT_EMI_DEBUG_BUF_SIZE (32*1024)
#endif

static struct proc_dir_entry *gWmtDbgEntry;
COEX_BUF gCoexBuf;
static UINT8 gEmiBuf[WMT_EMI_DEBUG_BUF_SIZE];
PUINT8 buf_emi;

static ssize_t wmt_dbg_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static ssize_t wmt_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);


static INT32 wmt_dbg_psm_ctrl(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_quick_sleep_ctrl(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_dsns_ctrl(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_hwver_get(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_inband_rst(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_chip_rst(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_func_ctrl(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_raed_chipid(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_wmt_dbg_level(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_stp_dbg_level(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_reg_read(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_reg_write(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_coex_test(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_assert_test(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_cmd_test_api(ENUM_WMTDRV_CMD_T cmd);
static INT32 wmt_dbg_rst_ctrl(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_ut_test(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_efuse_read(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_efuse_write(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_sdio_ctrl(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_stp_dbg_ctrl(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_stp_dbg_log_ctrl(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_wmt_assert_ctrl(INT32 par1, INT32 par2, INT32 par3);
#if CFG_CORE_INTERNAL_TXRX
static INT32 wmt_dbg_internal_lpbk_test(INT32 par1, INT32 par2, INT32 par3);
#endif
static INT32 wmt_dbg_stp_trigger_assert(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_ap_reg_read(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_ap_reg_write(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_set_mcu_clock(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_dbg_poll_cpupcr(INT32 par1, INT32 par2, INT32 par3);
#if CONSYS_ENALBE_SET_JTAG
static INT32 wmt_dbg_jtag_flag_ctrl(INT32 par1, INT32 par2, INT32 par3);
#endif
#if CFG_WMT_LTE_COEX_HANDLING
static INT32 wmt_dbg_lte_coex_test(INT32 par1, INT32 par2, INT32 par3);
#endif
#ifdef CONFIG_TRACING
static INT32 wmt_dbg_ftrace_dbg_log_ctrl(INT32 par1, INT32 par2, INT32 par3);
#endif

static const WMT_DEV_DBG_FUNC wmt_dev_dbg_func[] = {
	[0x0] = wmt_dbg_psm_ctrl,
	[0x1] = wmt_dbg_quick_sleep_ctrl,
	[0x2] = wmt_dbg_dsns_ctrl,
	[0x3] = wmt_dbg_hwver_get,
	[0x4] = wmt_dbg_assert_test,
	[0x5] = wmt_dbg_inband_rst,
	[0x6] = wmt_dbg_chip_rst,
	[0x7] = wmt_dbg_func_ctrl,
	[0x8] = wmt_dbg_raed_chipid,
	[0x9] = wmt_dbg_wmt_dbg_level,
	[0xa] = wmt_dbg_stp_dbg_level,
	[0xb] = wmt_dbg_reg_read,
	[0xc] = wmt_dbg_reg_write,
	[0xd] = wmt_dbg_coex_test,
	[0xe] = wmt_dbg_rst_ctrl,
	[0xf] = wmt_dbg_ut_test,
	[0x10] = wmt_dbg_efuse_read,
	[0x11] = wmt_dbg_efuse_write,
	[0x12] = wmt_dbg_sdio_ctrl,
	[0x13] = wmt_dbg_stp_dbg_ctrl,
	[0x14] = wmt_dbg_stp_dbg_log_ctrl,
	[0x15] = wmt_dbg_wmt_assert_ctrl,
	[0x16] = wmt_dbg_stp_trigger_assert,
	[0x17] = wmt_dbg_ap_reg_read,
	[0x18] = wmt_dbg_ap_reg_write,
	[0x19] = wmt_dbg_fwinfor_from_emi,
	[0x1a] = wmt_dbg_set_mcu_clock,
	[0x1b] = wmt_dbg_poll_cpupcr,
	[0x1c] = wmt_dbg_jtag_flag_ctrl,

#if CFG_WMT_LTE_COEX_HANDLING
	[0x1d] = wmt_dbg_lte_coex_test,
#endif
#ifdef CONFIG_TRACING
	[0x1e] = wmt_dbg_ftrace_dbg_log_ctrl,
#endif
};

static VOID wmt_dbg_fwinfor_print_buff(UINT32 len)
{
	UINT32 i = 0;
	UINT32 idx = 0;

	for (i = 0; i < len; i++) {
		buf_emi[idx] = gEmiBuf[i];
		if (gEmiBuf[i] == '\n') {
			pr_debug("%s", buf_emi);
			osal_memset(buf_emi, 0, BUF_LEN_MAX);
			idx = 0;
		} else {
			idx++;
			if (idx == BUF_LEN_MAX-1) {
				buf_emi[idx] = '\0';
				pr_debug("%s", buf_emi);
				osal_memset(buf_emi, 0, BUF_LEN_MAX);
				idx = 0;
			}
		}
	}
	if ((0 != idx) && (BUF_LEN_MAX > idx)) {
		buf_emi[idx] = '\0';
		pr_debug("%s", buf_emi);
		osal_memset(buf_emi, 0, BUF_LEN_MAX);
		idx = 0;
	}
}

INT32 wmt_dbg_psm_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
#if CFG_WMT_PS_SUPPORT
	if (0 == par2) {
		wmt_lib_ps_ctrl(0);
		WMT_INFO_FUNC("disable PSM\n");
	} else {
		par2 = (1 > par2 || 20000 < par2) ? STP_PSM_IDLE_TIME_SLEEP : par2;
		wmt_lib_ps_set_idle_time(par2);
		wmt_lib_ps_ctrl(1);
		WMT_WARN_FUNC("enable PSM, idle to sleep time = %d ms\n", par2);
	}
#else
	WMT_INFO_FUNC("WMT PS not supported\n");
#endif
	return 0;
}

INT32 wmt_dbg_quick_sleep_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
#if CFG_WMT_PS_SUPPORT
	UINT32 en_flag = par2;

	wmt_lib_quick_sleep_ctrl(en_flag);
#else
	WMT_WARN_FUNC("WMT PS not supported\n");
#endif
	return 0;
}

INT32 wmt_dbg_dsns_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
	if (WMTDSNS_FM_DISABLE <= par2 && WMTDSNS_MAX > par2) {
		WMT_INFO_FUNC("DSNS type (%d)\n", par2);
		mtk_wcn_wmt_dsns_ctrl(par2);
	} else {
		WMT_WARN_FUNC("invalid DSNS type\n");
	}
	return 0;
}

INT32 wmt_dbg_hwver_get(INT32 par1, INT32 par2, INT32 par3)
{
	WMT_INFO_FUNC("query chip version\n");
	mtk_wcn_wmt_hwver_get();
	return 0;
}

INT32 wmt_dbg_assert_test(INT32 par1, INT32 par2, INT32 par3)
{
	INT32 sec = 8;
	INT32 times = 0;

	if (0 == par3) {
		/* par2 = 0:  send assert command */
		/* par2 != 0: send exception command */
		return wmt_dbg_cmd_test_api(0 == par2 ? 0 : 1);
	} else if (1 == par3) {
		/* send noack command */
		return wmt_dbg_cmd_test_api(WMTDRV_CMD_NOACK_TEST);
	} else if (2 == par3) {
		/* warn reset test */
		return wmt_dbg_cmd_test_api(WMTDRV_CMD_WARNRST_TEST);
	} else if (3 == par3) {
		/* firmware trace test - for soc usage, not used in combo chip */
		return wmt_dbg_cmd_test_api(WMTDRV_CMD_FWTRACE_TEST);
	}

	times = par3;
	do {
		WMT_INFO_FUNC("Send Assert Command per 8 secs!!\n");
		wmt_dbg_cmd_test_api(0);
		osal_sleep_ms(sec * 1000);
	} while (--times);

	return 0;
}

INT32 wmt_dbg_cmd_test_api(ENUM_WMTDRV_CMD_T cmd)
{
	P_OSAL_OP pOp = NULL;
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	P_OSAL_SIGNAL pSignal;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}

	pSignal = &pOp->signal;

	pOp->op.opId = WMT_OPID_CMD_TEST;

	pSignal->timeoutValue = MAX_EACH_WMT_CMD;
	/*this test command should be run with usb cable connected, so no host awake is needed */
	/* wmt_lib_host_awake_get(); */
	wmt_lib_set_host_assert_info(WMTDRV_TYPE_WMT, 0, 1);
	switch (cmd) {
	case WMTDRV_CMD_ASSERT:
		pOp->op.au4OpData[0] = 0;
		break;
	case WMTDRV_CMD_EXCEPTION:
		pOp->op.au4OpData[0] = 1;
		break;
	case WMTDRV_CMD_NOACK_TEST:
		pOp->op.au4OpData[0] = 3;
		break;
	case WMTDRV_CMD_WARNRST_TEST:
		pOp->op.au4OpData[0] = 4;
		break;
	case WMTDRV_CMD_FWTRACE_TEST:
		pOp->op.au4OpData[0] = 5;
		break;
	default:
		if (WMTDRV_CMD_COEXDBG_00 <= cmd && WMTDRV_CMD_COEXDBG_15 >= cmd) {
			pOp->op.au4OpData[0] = 2;
			pOp->op.au4OpData[1] = cmd - 2;
		} else {
			pOp->op.au4OpData[0] = 0xff;
			pOp->op.au4OpData[1] = 0xff;
		}
		pOp->op.au4OpData[2] = (ULONG) gCoexBuf.buffer;
		pOp->op.au4OpData[3] = osal_sizeof(gCoexBuf.buffer);
		break;
	}
	WMT_INFO_FUNC("CMD_TEST, opid(%d), par(%lu, %lu)\n", pOp->op.opId, pOp->op.au4OpData[0],
		      pOp->op.au4OpData[1]);
	/*wake up chip first */
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return -1;
	}
	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();
	if ((cmd != WMTDRV_CMD_ASSERT) &&
	    (cmd != WMTDRV_CMD_EXCEPTION) &&
	    (cmd != WMTDRV_CMD_NOACK_TEST) &&
	    (cmd != WMTDRV_CMD_WARNRST_TEST) &&
		(cmd != WMTDRV_CMD_FWTRACE_TEST)) {
		if (MTK_WCN_BOOL_FALSE == bRet) {
			gCoexBuf.availSize = 0;
		} else {
			gCoexBuf.availSize = pOp->op.au4OpData[3];
			WMT_INFO_FUNC("gCoexBuf.availSize = %d\n", gCoexBuf.availSize);
		}
	}
	/* wmt_lib_host_awake_put(); */
	WMT_INFO_FUNC("CMD_TEST, opid (%d), par(%lu, %lu), ret(%d), result(%s)\n",
		      pOp->op.opId,
		      pOp->op.au4OpData[0],
		      pOp->op.au4OpData[1],
		      bRet, MTK_WCN_BOOL_FALSE == bRet ? "failed" : "succeed");

	return 0;
}

INT32 wmt_dbg_inband_rst(INT32 par1, INT32 par2, INT32 par3)
{
	if (0 == par2) {
		WMT_INFO_FUNC("inband reset test!!\n");
		mtk_wcn_stp_inband_reset();
	} else {
		WMT_INFO_FUNC("STP context reset in host side!!\n");
		mtk_wcn_stp_flush_context();
	}

	return 0;
}

INT32 wmt_dbg_chip_rst(INT32 par1, INT32 par2, INT32 par3)
{
	if (0 == par2) {
		if (mtk_wcn_stp_is_ready()) {
			WMT_INFO_FUNC("whole chip reset test\n");
			wmt_lib_cmb_rst(WMTRSTSRC_RESET_TEST);
		} else {
			WMT_INFO_FUNC("STP not ready , not to launch whole chip reset test\n");
		}
	} else if (1 == par2) {
		WMT_INFO_FUNC("chip hardware reset test\n");
		wmt_lib_hw_rst();
	} else {
		WMT_INFO_FUNC("chip software reset test\n");
		wmt_lib_sw_rst(1);
	}

	return 0;
}

INT32 wmt_dbg_func_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
	if (WMTDRV_TYPE_WMT > par2 || WMTDRV_TYPE_LPBK == par2) {
		if (0 == par3) {
			WMT_INFO_FUNC("function off test, type(%d)\n", par2);
			mtk_wcn_wmt_func_off(par2);
		} else {
			WMT_INFO_FUNC("function on test, type(%d)\n", par2);
			mtk_wcn_wmt_func_on(par2);
		}
	} else
		WMT_INFO_FUNC("function ctrl test, invalid type(%d)\n", par2);

	return 0;
}

INT32 wmt_dbg_raed_chipid(INT32 par1, INT32 par2, INT32 par3)
{
	WMT_INFO_FUNC("chip version = %d\n", wmt_lib_get_icinfo(WMTCHIN_MAPPINGHWVER));

	return 0;
}

INT32 wmt_dbg_wmt_dbg_level(INT32 par1, INT32 par2, INT32 par3)
{
	par2 = (WMT_LOG_ERR <= par2 && WMT_LOG_LOUD >= par2) ? par2 : WMT_LOG_INFO;
	wmt_lib_dbg_level_set(par2);
	WMT_INFO_FUNC("set wmt log level to %d\n", par2);

	return 0;
}

INT32 wmt_dbg_stp_dbg_level(INT32 par1, INT32 par2, INT32 par3)
{
	par2 = (0 <= par2 && 4 >= par2) ? par2 : 2;
	mtk_wcn_stp_dbg_level(par2);
	WMT_INFO_FUNC("set stp log level to %d\n", par2);

	return 0;
}

INT32 wmt_dbg_reg_read(INT32 par1, INT32 par2, INT32 par3)
{
	/* par2-->register address */
	/* par3-->register mask */
	UINT32 value = 0x0;
	UINT32 iRet = -1;
#if 0
	DISABLE_PSM_MONITOR();
	iRet = wmt_core_reg_rw_raw(0, par2, &value, par3);
	ENABLE_PSM_MONITOR();
#endif
	iRet = wmt_lib_reg_rw(0, par2, &value, par3);
	WMT_INFO_FUNC("read combo chip register (0x%08x) with mask (0x%08x) %s, value = 0x%08x\n",
		      par2, par3, iRet != 0 ? "failed" : "succeed", iRet != 0 ? -1 : value);

	return 0;
}

INT32 wmt_dbg_reg_write(INT32 par1, INT32 par2, INT32 par3)
{
	/* par2-->register address */
	/* par3-->value to set */
	UINT32 iRet = -1;
#if 0
	DISABLE_PSM_MONITOR();
	iRet = wmt_core_reg_rw_raw(1, par2, &par3, 0xffffffff);
	ENABLE_PSM_MONITOR();
#endif
	iRet = wmt_lib_reg_rw(1, par2, &par3, 0xffffffff);
	WMT_INFO_FUNC("write combo chip register (0x%08x) with value (0x%08x) %s\n",
		      par2, par3, iRet != 0 ? "failed" : "succeed");

	return 0;
}

INT32 wmt_dbg_efuse_read(INT32 par1, INT32 par2, INT32 par3)
{
	/* par2-->efuse address */
	/* par3-->register mask */
	UINT32 value = 0x0;
	UINT32 iRet = -1;

	iRet = wmt_lib_efuse_rw(0, par2, &value, par3);
	WMT_INFO_FUNC("read combo chip efuse (0x%08x) with mask (0x%08x) %s, value = 0x%08x\n",
		      par2, par3, iRet != 0 ? "failed" : "succeed", iRet != 0 ? -1 : value);

	return 0;
}

INT32 wmt_dbg_efuse_write(INT32 par1, INT32 par2, INT32 par3)
{
	/* par2-->efuse address */
	/* par3-->value to set */
	UINT32 iRet = -1;

	iRet = wmt_lib_efuse_rw(1, par2, &par3, 0xffffffff);
	WMT_INFO_FUNC("write combo chip efuse (0x%08x) with value (0x%08x) %s\n",
		      par2, par3, iRet != 0 ? "failed" : "succeed");

	return 0;
}

INT32 wmt_dbg_sdio_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
/*remove sdio card detect/remove control because of btif is used*/
#if 0
	INT32 iRet = -1;

	iRet = wmt_lib_sdio_ctrl(0 != par2 ? 1 : 0);
	WMT_INFO_FUNC("ctrl SDIO function %s\n", 0 == iRet ? "succeed" : "failed");
#endif
	return 0;
}

INT32 wmt_dbg_stp_dbg_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
	if (1 < par2) {
		mtk_wcn_stp_dbg_dump_package();
		return 0;
	}

	WMT_INFO_FUNC("%s stp debug function\n", 0 == par2 ? "disable" : "enable");

	if (0 == par2)
		mtk_wcn_stp_dbg_disable();
	else if (1 == par2)
		mtk_wcn_stp_dbg_enable();

	return 0;
}

INT32 wmt_dbg_stp_dbg_log_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
	mtk_wcn_stp_dbg_log_ctrl(0 != par2 ? 1 : 0);

	return 0;
}

INT32 wmt_dbg_wmt_assert_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
	mtk_wcn_stp_coredump_flag_ctrl(0 != par2 ? 1 : 0);

	return 0;
}

INT32 wmt_dbg_fwinfor_from_emi(INT32 par1, INT32 par2, INT32 par3)
{
	UINT32 offset = 0;
	UINT32 len = 0;
	UINT32 *pAddr = NULL;
	UINT32 cur_idx_pagedtrace;
	static UINT32 prev_idx_pagedtrace;
	MTK_WCN_BOOL isBreak = MTK_WCN_BOOL_TRUE;

	offset = par2;
	len = par3;

	buf_emi = kmalloc(sizeof(UINT8) * BUF_LEN_MAX, GFP_KERNEL);
	if (!buf_emi) {
			WMT_ERR_FUNC("buf kmalloc memory fail\n");
			return 0;
		}
	osal_memset(buf_emi, 0, BUF_LEN_MAX);
	osal_memset(&gEmiBuf[0], 0, WMT_EMI_DEBUG_BUF_SIZE);
	wmt_lib_get_fwinfor_from_emi(0, offset, &gEmiBuf[0], 0x100);

	if (offset == 1) {
		do {
			pAddr = (PUINT32) wmt_plat_get_emi_virt_add(0x24);
			cur_idx_pagedtrace = *pAddr;

			if (cur_idx_pagedtrace > prev_idx_pagedtrace) {
				len = cur_idx_pagedtrace - prev_idx_pagedtrace;
				wmt_lib_get_fwinfor_from_emi(1, prev_idx_pagedtrace, &gEmiBuf[0], len);
				wmt_dbg_fwinfor_print_buff(len);
				prev_idx_pagedtrace = cur_idx_pagedtrace;
			}

			if (cur_idx_pagedtrace < prev_idx_pagedtrace) {
				if (prev_idx_pagedtrace >= 0x8000) {
					pr_debug("++ prev_idx_pagedtrace invalid ...++\n\\n");
					prev_idx_pagedtrace = 0x8000 - 1;
					continue;
				}

				len = 0x8000 - prev_idx_pagedtrace - 1;
				wmt_lib_get_fwinfor_from_emi(1, prev_idx_pagedtrace, &gEmiBuf[0], len);
				pr_debug("\n\n -- CONNSYS paged trace ascii output (cont...) --\n\n");
				wmt_dbg_fwinfor_print_buff(len);

				len = cur_idx_pagedtrace;
				wmt_lib_get_fwinfor_from_emi(1, 0x0, &gEmiBuf[0], len);
				pr_debug("\n\n -- CONNSYS paged trace ascii output (end) --\n\n");
				wmt_dbg_fwinfor_print_buff(len);
				prev_idx_pagedtrace = cur_idx_pagedtrace;
			}
			msleep(100);
		} while (isBreak);
	}

	pr_debug("\n\n -- control word --\n\n");
	wmt_dbg_fwinfor_print_buff(256);
	if (len > 1024 * 4)
		len = 1024 * 4;

	WMT_WARN_FUNC("get fw infor from emi at offset(0x%x),len(0x%x)\n", offset, len);
	osal_memset(&gEmiBuf[0], 0, WMT_EMI_DEBUG_BUF_SIZE);
	wmt_lib_get_fwinfor_from_emi(1, offset, &gEmiBuf[0], len);

	pr_debug("\n\n -- paged trace hex output --\n\n");
	wmt_dbg_fwinfor_print_buff(len);
	pr_debug("\n\n -- paged trace ascii output --\n\n");
	wmt_dbg_fwinfor_print_buff(len);
	kfree(buf_emi);

	return 0;
}

INT32 wmt_dbg_stp_trigger_assert(INT32 par1, INT32 par2, INT32 par3)
{
	wmt_lib_btm_cb(BTM_TRIGGER_STP_ASSERT_OP);

	return 0;
}

static INT32 wmt_dbg_ap_reg_read(INT32 par1, INT32 par2, INT32 par3)
{
	ULONG value = 0x0;

	WMT_INFO_FUNC("AP register read, reg address:0x%x\n", par2);
	value = *((volatile unsigned long *)(unsigned long)par2);
	WMT_INFO_FUNC("AP register read, reg address:0x%x, value:0x%lx\n", par2, value);

	return 0;
}

static INT32 wmt_dbg_ap_reg_write(INT32 par1, INT32 par2, INT32 par3)
{
	ULONG value = 0x0;

	WMT_INFO_FUNC("AP register write, reg address:0x%x, value:0x%x\n", par2, par3);

	*((volatile unsigned long *)(unsigned long)par2) = (UINT32) par3;
	mb();
	value = *((volatile unsigned long *)(unsigned long)par2);
	WMT_INFO_FUNC("AP register write done, value after write:0x%lx\n", value);

	return 0;
}

#ifdef CONFIG_TRACING
static INT32 wmt_dbg_ftrace_dbg_log_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
	WMT_INFO_FUNC("%s ftrace print!!\n", 0 == par2 ? "disable" : "enable");

	return osal_ftrace_print_ctrl(0 == par2 ? 0 : 1);
}
#endif

INT32 wmt_dbg_coex_test(INT32 par1, INT32 par2, INT32 par3)
{
	WMT_INFO_FUNC("coexistance test cmd!!\n");

	return wmt_dbg_cmd_test_api(par2 + WMTDRV_CMD_COEXDBG_00);
}

INT32 wmt_dbg_rst_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
	WMT_INFO_FUNC("%s audo rst\n", 0 == par2 ? "disable" : "enable");
	mtk_wcn_stp_set_auto_rst(0 == par2 ? 0 : 1);

	return 0;
}

INT32 wmt_dbg_ut_test(INT32 par1, INT32 par2, INT32 par3)
{
	INT32 i = 0;
	INT32 j = 0;
	INT32 iRet = 0;

	i = 20;
	while ((i--) > 0) {
		WMT_INFO_FUNC("#### UT WMT and STP Function On/Off .... %d\n", i);
		j = 10;
		while ((j--) > 0) {
			WMT_INFO_FUNC("#### BT  On .... (%d, %d)\n", i, j);
			iRet = mtk_wcn_wmt_func_on(WMTDRV_TYPE_BT);
			if (iRet == MTK_WCN_BOOL_FALSE)
				break;
			WMT_INFO_FUNC("#### GPS On .... (%d, %d)\n", i, j);
			iRet = mtk_wcn_wmt_func_on(WMTDRV_TYPE_GPS);
			if (iRet == MTK_WCN_BOOL_FALSE)
				break;
			WMT_INFO_FUNC("#### FM  On .... (%d, %d)\n", i, j);
			iRet = mtk_wcn_wmt_func_on(WMTDRV_TYPE_FM);
			if (iRet == MTK_WCN_BOOL_FALSE)
				break;
			WMT_INFO_FUNC("#### WIFI On .... (%d, %d)\n", i, j);
			iRet = mtk_wcn_wmt_func_on(WMTDRV_TYPE_WIFI);
			if (iRet == MTK_WCN_BOOL_FALSE)
				break;
			WMT_INFO_FUNC("#### ANT On .... (%d, %d)\n", i, j);
			iRet = mtk_wcn_wmt_func_on(WMTDRV_TYPE_ANT);
			if (iRet == MTK_WCN_BOOL_FALSE)
				break;

			WMT_INFO_FUNC("#### BT  Off .... (%d, %d)\n", i, j);
			iRet = mtk_wcn_wmt_func_off(WMTDRV_TYPE_BT);
			if (iRet == MTK_WCN_BOOL_FALSE)
				break;

			WMT_INFO_FUNC("#### GPS  Off ....(%d, %d)\n", i, j);
			iRet = mtk_wcn_wmt_func_off(WMTDRV_TYPE_GPS);
			if (iRet == MTK_WCN_BOOL_FALSE)
				break;

			WMT_INFO_FUNC("#### FM  Off .... (%d, %d)\n", i, j);
			iRet = mtk_wcn_wmt_func_off(WMTDRV_TYPE_FM);
			if (iRet == MTK_WCN_BOOL_FALSE)
				break;
			WMT_INFO_FUNC("#### WIFI  Off ....(%d, %d)\n", i, j);
			iRet = mtk_wcn_wmt_func_off(WMTDRV_TYPE_WIFI);
			if (iRet == MTK_WCN_BOOL_FALSE)
				break;
			WMT_INFO_FUNC("#### ANT  Off ....(%d, %d)\n", i, j);
			iRet = mtk_wcn_wmt_func_off(WMTDRV_TYPE_ANT);
			if (iRet == MTK_WCN_BOOL_FALSE)
				break;
		}

		if (iRet == MTK_WCN_BOOL_FALSE)
			break;
	}
	if (iRet == MTK_WCN_BOOL_FALSE)
		WMT_INFO_FUNC("#### UT FAIL!!\n");
	else
		WMT_INFO_FUNC("#### UT PASS!!\n");

	return iRet;
}

#if CFG_CORE_INTERNAL_TXRX
struct lpbk_package {
	LONG payload_length;
	UINT8 out_payload[2048];
	UINT8 in_payload[2048];
};

static INT32 wmt_internal_loopback(INT32 count, INT32 max)
{
	INT32 ret = 0;
	INT32 loop;
	INT32 offset;
	struct lpbk_package lpbk_buffer;
	P_OSAL_OP pOp;
	P_OSAL_SIGNAL pSignal = NULL;

	for (loop = 0; loop < count; loop++) {
		/* <1> init buffer */
		osal_memset((PVOID)&lpbk_buffer, 0, sizeof(struct lpbk_package));
		lpbk_buffer.payload_length = max;
		for (offset = 0; offset < max; offset++)
			lpbk_buffer.out_payload[offset] = (offset + 1) & 0xFF;/*for test use: begin from 1 */

		memcpy(&gLpbkBuf[0], &lpbk_buffer.out_payload[0], max);

		pOp = wmt_lib_get_free_op();
		if (!pOp) {
			WMT_WARN_FUNC("get_free_lxop fail\n");
			ret = -1;
			break;
		}
		pSignal = &pOp->signal;
		pOp->op.opId = WMT_OPID_LPBK;
		pOp->op.au4OpData[0] = lpbk_buffer.payload_length;	/* packet length */
		pOp->op.au4OpData[1] = (UINT32) &gLpbkBuf[0];
		pSignal->timeoutValue = MAX_EACH_WMT_CMD;
		WMT_INFO_FUNC("OPID(%d) type(%d) start\n", pOp->op.opId, pOp->op.au4OpData[0]);
		if (DISABLE_PSM_MONITOR()) {
			WMT_ERR_FUNC("wake up failed,OPID(%d) type(%d) abort\n", pOp->op.opId,
					pOp->op.au4OpData[0]);
			wmt_lib_put_op_to_free_queue(pOp);
			ret = -2;
		}

		ret = wmt_lib_put_act_op(pOp);
		ENABLE_PSM_MONITOR();
		if (MTK_WCN_BOOL_FALSE == ret) {
			WMT_WARN_FUNC("OPID(%d) type(%d)fail\n", pOp->op.opId, pOp->op.au4OpData[0]);
			ret = -3;
			break;
		}
		WMT_INFO_FUNC("OPID(%d) length(%d) ok\n", pOp->op.opId, pOp->op.au4OpData[0]);

		memcpy(&lpbk_buffer.in_payload[0], &gLpbkBuf[0], max);

		ret = pOp->op.au4OpData[0];
		/*<3> compare result */
		if (memcmp(lpbk_buffer.in_payload, lpbk_buffer.out_payload, lpbk_buffer.payload_length)) {
			WMT_INFO_FUNC("[%s] WMT_TEST_LPBK_CMD payload compare error\n", __func__);
			ret = -4;
			break;
		}
		WMT_ERR_FUNC("[%s] exec WMT_TEST_LPBK_CMD succeed(loop = %d, size = %ld)\n", __func__, loop,
			     lpbk_buffer.payload_length);
	}

	if (loop != count)
		WMT_ERR_FUNC("fail at loop(%d) buf_length(%d)\n", loop, max);

	return ret;
}

INT32 wmt_dbg_internal_lpbk_test(INT32 par1, INT32 par2, INT32 par3)
{
	UINT32 count;
	UINT32 length;

	count = par1;
	length = par2;

	WMT_INFO_FUNC("count[%d],length[%d]\n", count, length);

	wmt_core_lpbk_do_stp_init();

	wmt_internal_loopback(count, length);

	wmt_core_lpbk_do_stp_deinit();
	return 0;
}
#endif /* CFG_CORE_INTERNAL_TXRX */

static INT32 wmt_dbg_set_mcu_clock(INT32 par1, INT32 par2, INT32 par3)
{
	INT32 ret = 0;
	P_OSAL_OP pOp;
	P_OSAL_SIGNAL pSignal = NULL;
	UINT32 kind = 0;

	kind = par2;
	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_WARN_FUNC("get_free_lxop fail\n");
		return -1;
	}
	pSignal = &pOp->signal;
	pOp->op.opId = WMT_OPID_SET_MCU_CLK;
	pOp->op.au4OpData[0] = kind;
	pSignal->timeoutValue = MAX_EACH_WMT_CMD;

	WMT_INFO_FUNC("OPID(%d) kind(%d) start\n", pOp->op.opId, pOp->op.au4OpData[0]);
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed,OPID(%d) kind(%d) abort\n", pOp->op.opId, pOp->op.au4OpData[0]);
		wmt_lib_put_op_to_free_queue(pOp);
		return -2;
	}

	ret = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();
	if (MTK_WCN_BOOL_FALSE == ret) {
		WMT_WARN_FUNC("OPID(%d) kind(%d)fail(%d)\n", pOp->op.opId, pOp->op.au4OpData[0], ret);
		return -3;
	}
	WMT_INFO_FUNC("OPID(%d) kind(%d) ok\n", pOp->op.opId, pOp->op.au4OpData[0]);

	return ret;
}

static INT32 wmt_dbg_poll_cpupcr(INT32 par1, INT32 par2, INT32 par3)
{
	UINT32 count = 0;
	UINT16 sleep = 0;
	UINT16 toAee = 0;

	count = par2;
	sleep = (par3 & 0xF0) >> 4;
	toAee = (par3 & 0x0F);

	WMT_INFO_FUNC("polling count[%d],polling sleep[%d],toaee[%d]\n", count, sleep, toAee);
	stp_dbg_poll_cpupcr(count, sleep, toAee);

	return 0;
}

#if CONSYS_ENALBE_SET_JTAG
static INT32 wmt_dbg_jtag_flag_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
	UINT32 en_flag = par2;

	wmt_lib_jtag_flag_set(en_flag);

	return 0;
}
#endif

#if CFG_WMT_LTE_COEX_HANDLING
static INT32 wmt_dbg_lte_to_wmt_test(UINT32 opcode, UINT32 msg_len)
{
	ipc_ilm_t ilm;
	local_para_struct *p_buf_str;
	INT32 i = 0;
	INT32 iRet = -1;

	WMT_INFO_FUNC("opcode(0x%02x),msg_len(%d)\n", opcode, msg_len);
	p_buf_str = osal_malloc(osal_sizeof(local_para_struct) + msg_len);
	if (NULL == p_buf_str) {
		WMT_ERR_FUNC("kmalloc for local para ptr structure failed.\n");
		return -1;
	}
	p_buf_str->msg_len = msg_len;
	for (i = 0; i < msg_len; i++)
		p_buf_str->data[i] = i;

	ilm.local_para_ptr = p_buf_str;
	ilm.msg_id = opcode;

	iRet = wmt_lib_handle_idc_msg(&ilm);
	osal_free(p_buf_str);
	return iRet;

}

static INT32 wmt_dbg_lte_coex_test(INT32 par1, INT32 par2, INT32 par3)
{
	PUINT8 local_buffer = NULL;
	UINT32 handle_len;
	INT32 iRet = -1;

	static UINT8 wmt_to_lte_test_evt1[] = { 0x02, 0x16, 0x0d, 0x00,
		0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
		0xa, 0xb
	};
	static UINT8 wmt_to_lte_test_evt2[] = { 0x02, 0x16, 0x09, 0x00,
		0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
	};
	static UINT8 wmt_to_lte_test_evt3[] = { 0x02, 0x16, 0x02, 0x00,
		0x02, 0xff
	};
	static UINT8 wmt_to_lte_test_evt4[] = { 0x02, 0x16, 0x0d, 0x00,
		0x03, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
		0xa, 0xb
	};

	local_buffer = kmalloc(512, GFP_KERNEL);
	if (!local_buffer) {
		WMT_ERR_FUNC("local_buffer kmalloc memory fail\n");
		return 0;
	}

	if (par2 == 1) {
		handle_len = wmt_idc_msg_to_lte_handing_for_test(&wmt_to_lte_test_evt1[0],
				osal_sizeof(wmt_to_lte_test_evt1));
		if (handle_len != osal_sizeof(wmt_to_lte_test_evt1)) {
			WMT_ERR_FUNC("par2=1,wmt send to lte msg fail:handle_len(%d),buff_len(%zu)\n",
					handle_len, osal_sizeof(wmt_to_lte_test_evt1));
		} else
			WMT_INFO_FUNC("par2=1,wmt send to lte msg OK! send_len(%d)\n", handle_len);
	}
	if (par2 == 2) {
		osal_memcpy(&local_buffer[0], &wmt_to_lte_test_evt1[0], osal_sizeof(wmt_to_lte_test_evt1));
		osal_memcpy(&local_buffer[osal_sizeof(wmt_to_lte_test_evt1)], &wmt_to_lte_test_evt2[0],
				osal_sizeof(wmt_to_lte_test_evt2));

		handle_len = wmt_idc_msg_to_lte_handing_for_test(&local_buffer[0],
				osal_sizeof(wmt_to_lte_test_evt1) + osal_sizeof(wmt_to_lte_test_evt2));
		if (handle_len != osal_sizeof(wmt_to_lte_test_evt1) + osal_sizeof(wmt_to_lte_test_evt2)) {
			WMT_ERR_FUNC("par2=2,wmt send to lte msg fail:handle_len(%d),buff_len(%zu)\n",
					handle_len,
					osal_sizeof(wmt_to_lte_test_evt1) +
					osal_sizeof(wmt_to_lte_test_evt2));
		} else
			WMT_INFO_FUNC("par2=1,wmt send to lte msg OK! send_len(%d)\n", handle_len);
	}
	if (par2 == 3) {
		osal_memcpy(&local_buffer[0], &wmt_to_lte_test_evt1[0], osal_sizeof(wmt_to_lte_test_evt1));
		osal_memcpy(&local_buffer[osal_sizeof(wmt_to_lte_test_evt1)], &wmt_to_lte_test_evt2[0],
				osal_sizeof(wmt_to_lte_test_evt2));
		osal_memcpy(&local_buffer[osal_sizeof(wmt_to_lte_test_evt1) +
				osal_sizeof(wmt_to_lte_test_evt2)],
				&wmt_to_lte_test_evt3[0], osal_sizeof(wmt_to_lte_test_evt3));

		handle_len = wmt_idc_msg_to_lte_handing_for_test(&local_buffer[0],
				osal_sizeof(wmt_to_lte_test_evt1) +
				osal_sizeof(wmt_to_lte_test_evt2) +
				osal_sizeof(wmt_to_lte_test_evt3));
		if (handle_len != osal_sizeof(wmt_to_lte_test_evt1) +
				osal_sizeof(wmt_to_lte_test_evt2) +
				osal_sizeof(wmt_to_lte_test_evt3)) {
			WMT_ERR_FUNC("par2=3,wmt send to lte msg fail:handle_len(%d),buff_len(%zu)\n",
					handle_len,
					osal_sizeof(wmt_to_lte_test_evt1) +
					osal_sizeof(wmt_to_lte_test_evt2) +
					osal_sizeof(wmt_to_lte_test_evt3));
		} else
			WMT_INFO_FUNC("par3=1,wmt send to lte msg OK! send_len(%d)\n", handle_len);
	}
	if (par2 == 4) {
		handle_len = wmt_idc_msg_to_lte_handing_for_test(&wmt_to_lte_test_evt4[0],
				osal_sizeof(wmt_to_lte_test_evt4));
		if (handle_len != osal_sizeof(wmt_to_lte_test_evt4)) {
			WMT_ERR_FUNC("par2=1,wmt send to lte msg fail:handle_len(%d),buff_len(%zu)\n",
					handle_len, osal_sizeof(wmt_to_lte_test_evt4));
		} else
			WMT_INFO_FUNC("par2=1,wmt send to lte msg OK! send_len(%d)\n", handle_len);
	}
	if (par2 == 5) {
		if (par3 >= 1024)
			par3 = 1024;
		iRet = wmt_dbg_lte_to_wmt_test(IPC_MSG_ID_EL1_LTE_DEFAULT_PARAM_IND, par3);
		WMT_INFO_FUNC("IPC_MSG_ID_EL1_LTE_DEFAULT_PARAM_IND test result(%d)\n", iRet);
	}
	if (par2 == 6) {
		if (par3 >= 1024)
			par3 = 1024;
		iRet = wmt_dbg_lte_to_wmt_test(IPC_MSG_ID_EL1_LTE_OPER_FREQ_PARAM_IND, par3);
		WMT_INFO_FUNC("IPC_MSG_ID_EL1_LTE_OPER_FREQ_PARAM_IND test result(%d)\n", iRet);
	}
	if (par2 == 7) {
		if (par3 >= 1024)
			par3 = 1024;
		iRet = wmt_dbg_lte_to_wmt_test(IPC_MSG_ID_EL1_WIFI_MAX_PWR_IND, par3);
		WMT_INFO_FUNC("IPC_MSG_ID_EL1_WIFI_MAX_PWR_IND test result(%d)\n", iRet);
	}
	if (par2 == 8) {
		if (par3 >= 1024)
			par3 = 1024;
		iRet = wmt_dbg_lte_to_wmt_test(IPC_MSG_ID_EL1_LTE_TX_IND, par3);
		WMT_INFO_FUNC("IPC_MSG_ID_EL1_LTE_TX_IND test result(%d)\n", iRet);
	}
	if (par2 == 9) {
		if (par3 > 0)
			wmt_core_set_flag_for_test(1);
		else
			wmt_core_set_flag_for_test(0);
	}

	kfree(local_buffer);

	return 0;
}
#endif /* CFG_WMT_LTE_COEX_HANDLING */

ssize_t wmt_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	INT32 retval = 0;
	INT32 i_ret = 0;
	PINT8 warn_msg = "no data available, please run echo 15 xx > /proc/driver/wmt_psm first\n";

	if (*f_pos > 0)
		retval = 0;
	else {
		/*len = sprintf(page, "%d\n", g_psm_enable); */
		if (gCoexBuf.availSize <= 0) {
			WMT_INFO_FUNC
			    ("no data available, please run echo 15 xx > /proc/driver/wmt_psm first\n");
			retval = osal_strlen(warn_msg) + 1;
			if (count < retval)
				retval = count;
			i_ret = copy_to_user(buf, warn_msg, retval);
			if (i_ret) {
				WMT_ERR_FUNC("copy to buffer failed, ret:%d\n", retval);
				retval = -EFAULT;
				goto err_exit;
			}
			*f_pos += retval;
		} else {
			INT32 i = 0;
			INT32 len = 0;
			INT8 msg_info[512];
			INT32 max_num = 0;

			/*we do not check page buffer, because there are only 100 bytes in g_coex_buf,
			 * no reason page buffer is not enough,
			 * a bomb is placed here on unexpected condition */
			WMT_INFO_FUNC("%d bytes available\n", gCoexBuf.availSize);
			max_num =
			    ((osal_sizeof(msg_info) >
			      count ? osal_sizeof(msg_info) : count) - 1) / 5;

			if (max_num > gCoexBuf.availSize) {
				max_num = gCoexBuf.availSize;
			} else {
				WMT_INFO_FUNC
				    ("round to %d bytes due to local buffer size limitation\n",
				     max_num);
			}

			for (i = 0; i < max_num; i++)
				len += osal_sprintf(msg_info + len, "0x%02x ", gCoexBuf.buffer[i]);

			len += osal_sprintf(msg_info + len, "\n");
			retval = len;

			i_ret = copy_to_user(buf, msg_info, retval);
			if (i_ret) {
				WMT_ERR_FUNC("copy to buffer failed, ret:%d\n", retval);
				retval = -EFAULT;
				goto err_exit;
			}
			*f_pos += retval;
		}
	}
	gCoexBuf.availSize = 0;

err_exit:
	return retval;
}

ssize_t wmt_dbg_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos)
{
	INT8 buf[256];
	PINT8 pBuf;
	ULONG len = count;
	INT32 x = 0, y = 0, z = 0;
	PINT8 pToken = NULL;
	PINT8 pDelimiter = " \t";
	LONG res;

	WMT_INFO_FUNC("write parameter len = %d\n\r", (INT32) len);
	if (len >= osal_sizeof(buf)) {
		WMT_ERR_FUNC("input handling fail!\n");
		len = osal_sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	WMT_INFO_FUNC("write parameter data = %s\n\r", buf);

	pBuf = buf;
	pToken = osal_strsep(&pBuf, pDelimiter);
	osal_strtol(pToken, 16, &res);
	x = NULL != pToken ? (INT32)res : 0;

	pToken = osal_strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		y = (INT32)res;
		WMT_INFO_FUNC("y = 0x%08x\n\r", y);
	} else {
		y = 3000;
		/*efuse, register read write default value */
		if (0x11 == x || 0x12 == x || 0x13 == x)
			y = 0x80000000;
	}

	pToken = osal_strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		z = (INT32)res;
	} else {
		z = 10;
		/*efuse, register read write default value */
		if (0x11 == x || 0x12 == x || 0x13 == x)
			z = 0xffffffff;
	}

	WMT_INFO_FUNC("x(0x%08x), y(0x%08x), z(0x%08x)\n\r", x, y, z);

	if (osal_array_size(wmt_dev_dbg_func) > x && NULL != wmt_dev_dbg_func[x])
		(*wmt_dev_dbg_func[x]) (x, y, z);
	else
		WMT_WARN_FUNC("no handler defined for command id(0x%08x)\n\r", x);

	return len;
}

INT32 wmt_dev_dbg_setup(VOID)
{
	static const struct file_operations wmt_dbg_fops = {
		.owner = THIS_MODULE,
		.read = wmt_dbg_read,
		.write = wmt_dbg_write,
	};
	INT32 i_ret;

	gWmtDbgEntry = proc_create(WMT_DBG_PROCNAME, 0664, NULL, &wmt_dbg_fops);
	if (gWmtDbgEntry == NULL) {
		WMT_ERR_FUNC("Unable to create / wmt_aee proc entry\n\r");
		i_ret = -1;
	}

	return i_ret;
}

INT32 wmt_dev_dbg_remove(VOID)
{
	if (NULL != gWmtDbgEntry)
		proc_remove(gWmtDbgEntry);

#if CFG_WMT_PS_SUPPORT
	wmt_lib_ps_deinit();
#endif
	return 0;
}
