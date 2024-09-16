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

#include <linux/kernel.h>	/* GFP_KERNEL */
#include <linux/timer.h>	/* init_timer, add_time, del_timer_sync */
#include <linux/time.h>		/* gettimeofday */
#include <linux/delay.h>
#include <linux/slab.h>		/* kzalloc */
#include <linux/sched.h>	/* task's status */
#include <linux/vmalloc.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>

#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <net/genetlink.h>

#include <linux/zlib.h>
#include <linux/uaccess.h>
#include <linux/crc32.h>

#include "osal.h"
#include "stp_dbg.h"
#include "stp_dbg_combo.h"
#include "stp_dbg_soc.h"
/* #include "stp_btm.h" */
#include "btm_core.h"
#include "wmt_plat.h"
#include "wmt_detect.h"
#include "stp_sdio.h"
#include "stp_core.h"
#include "mtk_wcn_consys_hw.h"
#include "wmt_lib.h"


UINT32 gStpDbgLogOut;
UINT32 gStpDbgDumpType = STP_DBG_PKT;
INT32 gStpDbgDbgLevel = STP_DBG_LOG_INFO;
static CONSYS_STATE_DMP_INFO g_dmp_info;

MTKSTP_DBG_T *g_stp_dbg;

static OSAL_SLEEPABLE_LOCK g_dbg_nl_lock;

#define STP_DBG_FAMILY_NAME        "STP_DBG"
#define MAX_BIND_PROCESS    (4)
#ifdef WMT_PLAT_ALPS
#ifndef LOG_STP_DEBUG_DISABLE
#define STP_DBG_AEE_EXP_API (1)
#else
#define STP_DBG_AEE_EXP_API (0)
#endif
#else
#define STP_DBG_AEE_EXP_API (0)
#endif

#define STP_MAGIC_NUM (0xDEADFEED)

#ifndef GENL_ID_GENERATE
#define GENL_ID_GENERATE	0
#endif

enum {
	__STP_DBG_ATTR_INVALID,
	STP_DBG_ATTR_MSG,
	__STP_DBG_ATTR_MAX,
};
#define STP_DBG_ATTR_MAX       (__STP_DBG_ATTR_MAX - 1)

enum {
	__STP_DBG_COMMAND_INVALID,
	STP_DBG_COMMAND_BIND,
	STP_DBG_COMMAND_RESET,
	__STP_DBG_COMMAND_MAX,
};
#define MTK_WIFI_COMMAND_MAX    (__STP_DBG_COMMAND_MAX - 1)

/* attribute policy */
static struct nla_policy stp_dbg_genl_policy[STP_DBG_ATTR_MAX + 1] = {
	[STP_DBG_ATTR_MSG] = {.type = NLA_NUL_STRING},
};

static UINT32 stp_dbg_seqnum;
static INT32 num_bind_process;
static pid_t bind_pid[MAX_BIND_PROCESS];
static P_WCN_CORE_DUMP_T g_core_dump;
static P_STP_DBG_CPUPCR_T g_stp_dbg_cpupcr;
/* just show in log at present */
static P_STP_DBG_DMAREGS_T g_stp_dbg_dmaregs;

static VOID stp_dbg_core_dump_timeout_handler(timer_handler_arg arg);
static VOID stp_dbg_dump_emi_timeout_handler(timer_handler_arg arg);
static _osal_inline_ P_WCN_CORE_DUMP_T stp_dbg_core_dump_init(UINT32 timeout);
static _osal_inline_ INT32 stp_dbg_core_dump_deinit(P_WCN_CORE_DUMP_T dmp);
static _osal_inline_ INT32 stp_dbg_core_dump_check_end(PUINT8 buf, INT32 len);
static _osal_inline_ INT32 stp_dbg_core_dump_in(P_WCN_CORE_DUMP_T dmp, PUINT8 buf, INT32 len);
static _osal_inline_ INT32 stp_dbg_core_dump_post_handle(P_WCN_CORE_DUMP_T dmp);
static _osal_inline_ INT32 stp_dbg_core_dump_out(P_WCN_CORE_DUMP_T dmp, PPUINT8 pbuf, PINT32 plen);
static _osal_inline_ INT32 stp_dbg_core_dump_reset(P_WCN_CORE_DUMP_T dmp, UINT32 timeout);
static _osal_inline_ INT32 stp_dbg_core_dump_nl(P_WCN_CORE_DUMP_T dmp, PUINT8 buf, INT32 len);
static _osal_inline_ UINT32 stp_dbg_get_chip_id(VOID);
static _osal_inline_ INT32 stp_dbg_gzip_compressor(PVOID worker, PUINT8 in_buf, INT32 in_sz, PUINT8 out_buf,
				 PINT32 out_sz, INT32 finish);
static _osal_inline_ P_WCN_COMPRESSOR_T stp_dbg_compressor_init(PUINT8 name, INT32 L1_buf_sz, INT32 L2_buf_sz);
static _osal_inline_ INT32 stp_dbg_compressor_deinit(P_WCN_COMPRESSOR_T cprs);
static _osal_inline_ INT32 stp_dbg_compressor_in(P_WCN_COMPRESSOR_T cprs,
				PUINT8 buf, INT32 len, INT32 is_iobuf, INT32 finish);
static _osal_inline_ INT32 stp_dbg_compressor_out(P_WCN_COMPRESSOR_T cprs, PPUINT8 pbuf, PINT32 plen);
static _osal_inline_ INT32 stp_dbg_compressor_reset(P_WCN_COMPRESSOR_T cprs, UINT8 enable, WCN_COMPRESS_ALG_T type);
static _osal_inline_ VOID stp_dbg_dump_data(PUINT8 pBuf, PINT8 title, INT32 len);
static _osal_inline_ INT32 stp_dbg_dmp_in(MTKSTP_DBG_T *stp_dbg, PINT8 buf, INT32 len);
static _osal_inline_ INT32 stp_dbg_notify_btm_dmp_wq(MTKSTP_DBG_T *stp_dbg);
static _osal_inline_ INT32 stp_dbg_get_avl_entry_num(MTKSTP_DBG_T *stp_dbg);
static _osal_inline_ INT32 stp_dbg_fill_hdr(STP_DBG_HDR_T *hdr, INT32 type, INT32 ack, INT32 seq,
			      INT32 crc, INT32 dir, INT32 len, INT32 dbg_type);
static _osal_inline_ INT32 stp_dbg_add_pkt(MTKSTP_DBG_T *stp_dbg, STP_DBG_HDR_T *hdr, const PUINT8 body);
static INT32 stp_dbg_nl_bind(struct sk_buff *skb, struct genl_info *info);
static INT32 stp_dbg_nl_reset(struct sk_buff *skb, struct genl_info *info);
static _osal_inline_ INT32 stp_dbg_parser_assert_str(PINT8 str, ENUM_ASSERT_INFO_PARSER_TYPE type);
static _osal_inline_ P_STP_DBG_CPUPCR_T stp_dbg_cpupcr_init(VOID);
static _osal_inline_ VOID stp_dbg_cpupcr_deinit(P_STP_DBG_CPUPCR_T pCpupcr);
static _osal_inline_ P_STP_DBG_DMAREGS_T stp_dbg_dmaregs_init(VOID);
static _osal_inline_ VOID stp_dbg_dmaregs_deinit(P_STP_DBG_DMAREGS_T pDmaRegs);

INT32 __weak mtk_btif_rxd_be_blocked_flag_get(VOID)
{
	STP_DBG_PR_INFO("mtk_btif_rxd_be_blocked_flag_get is not define!!!\n");
	return 0;
}

/* operation definition */
static struct genl_ops stp_dbg_gnl_ops_array[] = {
	{
		.cmd = STP_DBG_COMMAND_BIND,
		.flags = 0,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0))
		.policy = stp_dbg_genl_policy,
#endif
		.doit = stp_dbg_nl_bind,
		.dumpit = NULL,
	},
	{
		.cmd = STP_DBG_COMMAND_RESET,
		.flags = 0,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0))
		.policy = stp_dbg_genl_policy,
#endif
		.doit = stp_dbg_nl_reset,
		.dumpit = NULL,
	},
};

static struct genl_family stp_dbg_gnl_family = {
	.id = GENL_ID_GENERATE,
	.hdrsize = 0,
	.name = STP_DBG_FAMILY_NAME,
	.version = 1,
	.maxattr = STP_DBG_ATTR_MAX,
	.ops = stp_dbg_gnl_ops_array,
	.n_ops = ARRAY_SIZE(stp_dbg_gnl_ops_array),
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
	.policy = stp_dbg_genl_policy,
#endif
};
/* stp_dbg_core_dump_timeout_handler - handler of coredump timeout
 * @ data - core dump object's pointer
 *
 * No return value
 */
static VOID stp_dbg_core_dump_timeout_handler(timer_handler_arg arg)
{
	stp_dbg_set_coredump_timer_state(CORE_DUMP_TIMEOUT);
	stp_btm_notify_coredump_timeout_wq(g_stp_dbg->btm);
	STP_DBG_PR_WARN(" coredump timer timeout, coredump maybe not finished successfully\n");
}

/* stp_dbg_dump_emi_timeout_handler - handler of emi dump timeout
 * @ data - core dump object's pointer
 *
 * No return value
 */
static VOID stp_dbg_dump_emi_timeout_handler(timer_handler_arg arg)
{
	STP_DBG_PR_ERR("dump emi timeout!\n");
	mtk_stp_notify_emi_dump_end();
}

/* stp_dbg_core_dump_init - create core dump sys
 * @ packet_num - core dump packet number unit 32k
 * @ timeout - core dump time out value
 *
 * Return object pointer if success, else NULL
 */
static _osal_inline_ P_WCN_CORE_DUMP_T stp_dbg_core_dump_init(UINT32 timeout)
{
	P_WCN_CORE_DUMP_T core_dmp = NULL;

	core_dmp = (P_WCN_CORE_DUMP_T) osal_malloc(sizeof(WCN_CORE_DUMP_T));
	if (!core_dmp) {
		STP_DBG_PR_ERR("alloc mem failed!\n");
		return NULL;
	}

	osal_memset(core_dmp, 0, sizeof(WCN_CORE_DUMP_T));

	core_dmp->dmp_timer.timeoutHandler = stp_dbg_core_dump_timeout_handler;
	core_dmp->dmp_timer.timeroutHandlerData = (ULONG)core_dmp;
	osal_timer_create(&core_dmp->dmp_timer);
	core_dmp->timeout = timeout;
	core_dmp->dmp_emi_timer.timeoutHandler = stp_dbg_dump_emi_timeout_handler;
	core_dmp->dmp_emi_timer.timeroutHandlerData = (ULONG)core_dmp;
	osal_timer_create(&core_dmp->dmp_emi_timer);

	osal_sleepable_lock_init(&core_dmp->dmp_lock);

	core_dmp->sm = CORE_DUMP_INIT;
	STP_DBG_PR_INFO("create coredump object OK!\n");

	return core_dmp;
}


/* stp_dbg_core_dump_deinit - destroy core dump object
 * @ dmp - pointer of object
 *
 * Retunr 0 if success, else error code
 */
static _osal_inline_ INT32 stp_dbg_core_dump_deinit(P_WCN_CORE_DUMP_T dmp)
{
	if (dmp) {
		if (dmp->p_head != NULL) {
			osal_free(dmp->p_head);
			dmp->p_head = NULL;
		}
		osal_sleepable_lock_deinit(&dmp->dmp_lock);
		osal_timer_stop(&dmp->dmp_timer);
		osal_timer_stop(&dmp->dmp_emi_timer);
		osal_free(dmp);
		dmp = NULL;
	}

	return 0;
}

INT32 stp_dbg_core_dump_deinit_gcoredump(VOID)
{
	stp_dbg_core_dump_deinit(g_core_dump);
	return 0;
}

static _osal_inline_ INT32 stp_dbg_core_dump_check_end(PUINT8 buf, INT32 len)
{
	if (strnstr(buf, "coredump end", len))
		return 1;
	else
		return 0;
}

static UINT32 stp_dbg_core_dump_header_init(P_WCN_CORE_DUMP_T dmp)
{
	dmp->head_len = 0;
	if (dmp->p_head == NULL) {
		dmp->p_head = osal_malloc(MAX_DUMP_HEAD_LEN);
		if (dmp->p_head == NULL) {
			STP_DBG_PR_ERR("alloc memory for head information failed\n");
			return -1;
		}
	}
	if (dmp->p_head != NULL)
		osal_memset(dmp->p_head, 0, MAX_DUMP_HEAD_LEN);

	return 0;
}

static UINT32 stp_dbg_core_dump_header_append(P_WCN_CORE_DUMP_T dmp, PUINT8 buf, INT32 len)
{
	INT32 tmp = 0;

	if ((dmp->p_head != NULL) && (dmp->head_len < (MAX_DUMP_HEAD_LEN - 1))) {
		tmp =
		    (dmp->head_len + len) >
		    (MAX_DUMP_HEAD_LEN - 1) ? (MAX_DUMP_HEAD_LEN - 1 - dmp->head_len) : len;
		osal_memcpy(dmp->p_head + dmp->head_len, buf, tmp);
		dmp->head_len += tmp;
		return tmp;
	}
	return 0;
}

/* stp_dbg_core_dump_in - add a packet to compressor buffer
 * @ dmp - pointer of object
 * @ buf - input buffer
 * @ len - data length
 *
 * Retunr 0 if success; return 1 if find end string; else error code
 */
static _osal_inline_ INT32 stp_dbg_core_dump_in(P_WCN_CORE_DUMP_T dmp, PUINT8 buf, INT32 len)
{
	INT32 ret = 0;

	if ((!dmp) || (!buf)) {
		STP_DBG_PR_ERR("invalid pointer!\n");
		return -1;
	}

	ret = osal_lock_sleepable_lock(&dmp->dmp_lock);
	if (ret) {
		STP_DBG_PR_ERR("--->lock dmp->dmp_lock failed, ret=%d\n", ret);
		return ret;
	}

	switch (dmp->sm) {
	case CORE_DUMP_INIT:
		stp_dbg_compressor_reset(dmp->compressor, 1, GZIP);
		stp_dbg_core_dump_header_init(dmp);
		/* show coredump start info on UI */
		/* osal_dbg_assert_aee("MT662x f/w coredump start", "MT662x firmware coredump start"); */
		/* parsing data, and check end srting */
		ret = stp_dbg_core_dump_check_end(buf, len);
		if (ret == 1) {
			STP_DBG_PR_INFO("core dump end!\n");
			dmp->sm = CORE_DUMP_DONE;
			stp_dbg_compressor_in(dmp->compressor, buf, len, 0, 0);
		} else {
			dmp->sm = CORE_DUMP_DOING;
			stp_dbg_compressor_in(dmp->compressor, buf, len, 0, 0);
		}
		break;

	case CORE_DUMP_DOING:
		/* parsing data, and check end srting */
		ret = stp_dbg_core_dump_check_end(buf, len);
		if (ret == 1) {
			STP_DBG_PR_INFO("core dump end!\n");
			dmp->sm = CORE_DUMP_DONE;
			stp_dbg_compressor_in(dmp->compressor, buf, len, 0, 0);
		} else {
			dmp->sm = CORE_DUMP_DOING;
			stp_dbg_compressor_in(dmp->compressor, buf, len, 0, 0);
		}
		break;

	case CORE_DUMP_DONE:
		stp_dbg_compressor_reset(dmp->compressor, 1, GZIP);
		osal_timer_stop(&dmp->dmp_timer);
		stp_dbg_compressor_in(dmp->compressor, buf, len, 0, 0);
		dmp->sm = CORE_DUMP_DOING;
		break;

	case CORE_DUMP_TIMEOUT:
		ret = -1;
		break;
	default:
		break;
	}

	stp_dbg_core_dump_header_append(dmp, buf, len);
	osal_unlock_sleepable_lock(&dmp->dmp_lock);

	return ret;
}

static _osal_inline_ INT32 stp_dbg_core_dump_post_handle(P_WCN_CORE_DUMP_T dmp)
{
#define INFO_HEAD ";CONSYS FW CORE, "
	INT32 ret = 0;
	INT32 tmp = 0;
	ENUM_STP_FW_ISSUE_TYPE issue_type;

	if ((dmp->p_head != NULL)
	    && ((osal_strnstr(dmp->p_head, "<ASSERT>", dmp->head_len)) != NULL ||
		stp_dbg_get_host_trigger_assert())) {
		PINT8 pStr = dmp->p_head;
		PINT8 pDtr = NULL;

		if (stp_dbg_get_host_trigger_assert())
			issue_type = STP_HOST_TRIGGER_FW_ASSERT;
		else
			issue_type = STP_FW_ASSERT_ISSUE;
		STP_DBG_PR_INFO("dmp->head_len = %d\n", dmp->head_len);
		/*parse f/w assert additional informationi for f/w's analysis */
		ret = stp_dbg_set_fw_info(dmp->p_head, dmp->head_len, issue_type);
		if (ret) {
			STP_DBG_PR_ERR("set fw issue infor fail(%d),maybe fw warm reset...\n",
					 ret);
			stp_dbg_set_fw_info("Fw Warm reset", osal_strlen("Fw Warm reset"),
					    STP_FW_WARM_RST_ISSUE);
		}
		/* first package, copy to info buffer */
		osal_strcpy(&dmp->info[0], INFO_HEAD);

		/* set f/w assert information to warm reset */
		pStr = osal_strnstr(pStr, "<ASSERT>", dmp->head_len);
		if (pStr != NULL) {
			pDtr = osal_strchr(pStr, '-');
			if (pDtr != NULL) {
				tmp = STP_CORE_DUMP_INFO_SZ - osal_strlen(INFO_HEAD);
				tmp = ((pDtr - pStr) > tmp) ? tmp : (pDtr - pStr);
				osal_memcpy(&dmp->info[osal_strlen(INFO_HEAD)], pStr, tmp);
				dmp->info[osal_strlen(dmp->info) + 1] = '\0';
			} else {
				tmp = STP_CORE_DUMP_INFO_SZ - osal_strlen(INFO_HEAD);
				tmp = (dmp->head_len > tmp) ? tmp : dmp->head_len;
				osal_memcpy(&dmp->info[osal_strlen(INFO_HEAD)], pStr, tmp);
				dmp->info[STP_CORE_DUMP_INFO_SZ] = '\0';
			}
		}
	} else if ((dmp->p_head != NULL)
			&& ((osal_strnstr(dmp->p_head, "<EXCEPTION>", dmp->head_len) != NULL)
			|| (osal_strnstr(dmp->p_head, "ABT", dmp->head_len) != NULL))) {
		stp_dbg_set_fw_info(dmp->p_head, dmp->head_len, STP_FW_ABT);
		osal_strcpy(&dmp->info[0], INFO_HEAD);
		osal_memcpy(&dmp->info[osal_strlen(INFO_HEAD)], "Fw ABT Exception...",
			    osal_strlen("Fw ABT Exception..."));
		dmp->info[osal_strlen(INFO_HEAD) + osal_strlen("Fw ABT Exception...") + 1] = '\0';
	} else {
		STP_DBG_PR_INFO(" <ASSERT> string not found, dmp->head_len:%d\n", dmp->head_len);
		if (dmp->p_head == NULL)
			STP_DBG_PR_INFO(" dmp->p_head is NULL\n");
		else
			STP_DBG_PR_INFO(" dmp->p_head:%s\n", dmp->p_head);

		/* first package, copy to info buffer */
		osal_strcpy(&dmp->info[0], INFO_HEAD);
		/* set f/w assert information to warm reset */
		osal_memcpy(&dmp->info[osal_strlen(INFO_HEAD)], "Fw warm reset exception...",
			    osal_strlen("Fw warm reset exception..."));
		dmp->info[osal_strlen(INFO_HEAD) + osal_strlen("Fw warm reset exception...") + 1] =
		    '\0';

	}
	dmp->head_len = 0;

	/*set ret value to notify upper layer do dump flush operation */
	ret = 1;

	return ret;
}

/* stp_dbg_core_dump_out - get compressed data from compressor buffer
 * @ dmp - pointer of object
 * @ pbuf - target buffer's pointer
 * @ len - data length
 *
 * Retunr 0 if success;  else error code
 */
static _osal_inline_ INT32 stp_dbg_core_dump_out(P_WCN_CORE_DUMP_T dmp, PPUINT8 pbuf, PINT32 plen)
{
	INT32 ret = 0;

	if ((!dmp) || (!pbuf) || (!plen)) {
		STP_DBG_PR_ERR("invalid pointer!\n");
		return -1;
	}

	ret = osal_lock_sleepable_lock(&dmp->dmp_lock);
	if (ret) {
		STP_DBG_PR_ERR("--->lock dmp->dmp_lock failed, ret=%d\n", ret);
		return ret;
	}

	ret = stp_dbg_compressor_out(dmp->compressor, pbuf, plen);

	osal_unlock_sleepable_lock(&dmp->dmp_lock);

	return ret;
}

/* stp_dbg_core_dump_reset - reset core dump sys
 * @ dmp - pointer of object
 * @ timeout - core dump time out value
 *
 * Retunr 0 if success, else error code
 */
static _osal_inline_ INT32 stp_dbg_core_dump_reset(P_WCN_CORE_DUMP_T dmp, UINT32 timeout)
{
	if (!dmp) {
		STP_DBG_PR_ERR("invalid pointer!\n");
		return -1;
	}

	dmp->sm = CORE_DUMP_INIT;
	dmp->timeout = timeout;
	osal_timer_stop(&dmp->dmp_timer);
	osal_timer_stop(&dmp->dmp_emi_timer);
	osal_memset(dmp->info, 0, STP_CORE_DUMP_INFO_SZ + 1);

	stp_dbg_core_dump_deinit(dmp);
	g_core_dump = stp_dbg_core_dump_init(STP_CORE_DUMP_TIMEOUT);

	return 0;
}

#define ENABLE_F_TRACE 0
/* stp_dbg_core_dump_flush - Fulsh dump data and reset core dump sys
 *
 * Retunr 0 if success, else error code
 */
INT32 stp_dbg_core_dump_flush(INT32 rst, MTK_WCN_BOOL coredump_is_timeout)
{
	PUINT8 pbuf = NULL;
	INT32 len = 0;

	if (!g_core_dump) {
		STP_DBG_PR_ERR("invalid pointer!\n");
		return -1;
	}

	osal_lock_sleepable_lock(&g_core_dump->dmp_lock);
	stp_dbg_core_dump_post_handle(g_core_dump);
	osal_unlock_sleepable_lock(&g_core_dump->dmp_lock);
	stp_dbg_core_dump_out(g_core_dump, &pbuf, &len);
	STP_DBG_PR_INFO("buf 0x%zx, len %d\n", (SIZE_T) pbuf, len);

#if IS_ENABLED(CONFIG_MTK_AEE_AED)
	/* show coredump end info on UI */
	/* osal_dbg_assert_aee("MT662x f/w coredump end", "MT662x firmware coredump ends"); */
#if STP_DBG_AEE_EXP_API
#if ENABLE_F_TRACE
	aed_combo_exception_api(NULL, 0, (const PINT32)pbuf, len, (const PINT8)g_core_dump->info,
			DB_OPT_FTRACE);
#else
	aed_combo_exception(NULL, 0, (const PINT32)pbuf, len, (const PINT8)g_core_dump->info);
#endif
#endif

#endif
	/* reset */
	g_core_dump->count = 0;
	stp_dbg_compressor_deinit(g_core_dump->compressor);
	stp_dbg_core_dump_reset(g_core_dump, STP_CORE_DUMP_TIMEOUT);

	return 0;
}

static _osal_inline_ INT32 stp_dbg_core_dump_nl(P_WCN_CORE_DUMP_T dmp, PUINT8 buf, INT32 len)
{
	INT32 ret = 0;

	if ((!dmp) || (!buf)) {
		STP_DBG_PR_ERR("invalid pointer!\n");
		return -1;
	}

	ret = osal_lock_sleepable_lock(&dmp->dmp_lock);
	if (ret) {
		STP_DBG_PR_ERR("--->lock dmp->dmp_lock failed, ret=%d\n", ret);
		return ret;
	}

	switch (dmp->sm) {
	case CORE_DUMP_INIT:
		STP_DBG_PR_WARN("CONSYS coredump start, please wait up to %d minutes.\n",
				STP_CORE_DUMP_TIMEOUT/60000);
		stp_dbg_core_dump_header_init(dmp);
		/* check end srting */
		ret = stp_dbg_core_dump_check_end(buf, len);
		if (ret == 1) {
			STP_DBG_PR_INFO("core dump end!\n");
			osal_timer_stop(&dmp->dmp_timer);
			dmp->sm = CORE_DUMP_INIT;
		} else {
			dmp->sm = CORE_DUMP_DOING;
		}
		break;

	case CORE_DUMP_DOING:
		/* check end srting */
		ret = stp_dbg_core_dump_check_end(buf, len);
		if (ret == 1) {
			STP_DBG_PR_INFO("core dump end!\n");
			osal_timer_stop(&dmp->dmp_timer);
			dmp->sm = CORE_DUMP_INIT;
		} else {
			dmp->sm = CORE_DUMP_DOING;
		}
		break;

	case CORE_DUMP_DONE:
		osal_timer_stop(&dmp->dmp_timer);
		dmp->sm = CORE_DUMP_INIT;
		break;

	case CORE_DUMP_TIMEOUT:
		ret = 32;
		break;
	default:
		break;
	}

	/* Skip nl packet header */
	stp_dbg_core_dump_header_append(dmp, buf + NL_PKT_HEADER_LEN, len - NL_PKT_HEADER_LEN);
	osal_unlock_sleepable_lock(&dmp->dmp_lock);

	return ret;
}

INT32 stp_dbg_core_dump(INT32 dump_sink)
{
	ENUM_WMT_CHIP_TYPE chip_type;
	INT32 ret = 0;

	chip_type = wmt_detect_get_chip_type();
	switch (chip_type) {
	case WMT_CHIP_TYPE_COMBO:
		ret = stp_dbg_combo_core_dump(dump_sink);
		break;
	case WMT_CHIP_TYPE_SOC:
		ret = stp_dbg_soc_core_dump(dump_sink);
		break;
	default:
		STP_DBG_PR_ERR("error chip type(%d)\n", chip_type);
	}

	return ret;
}

static _osal_inline_ UINT32 stp_dbg_get_chip_id(VOID)
{
	ENUM_WMT_CHIP_TYPE chip_type;
	UINT32 chip_id = 0;

	chip_type = wmt_detect_get_chip_type();
	switch (chip_type) {
	case WMT_CHIP_TYPE_COMBO:
		chip_id = mtk_wcn_wmt_chipid_query();
		break;
	case WMT_CHIP_TYPE_SOC:
		chip_id = wmt_plat_get_soc_chipid();
		break;
	default:
		STP_DBG_PR_ERR("error chip type(%d)\n", chip_type);
	}

	return chip_id;
}

/* stp_dbg_trigger_collect_ftrace - this func can collect SYS_FTRACE
 *
 * Retunr 0 if success
 */
INT32 stp_dbg_trigger_collect_ftrace(PUINT8 pbuf, INT32 len)
{
	if (!pbuf) {
		STP_DBG_PR_ERR("Parameter error\n");
		return -1;
	}

	if (mtk_wcn_stp_coredump_start_get()) {
		STP_DBG_PR_ERR("assert has been triggered\n");
		return -1;
	}

	stp_dbg_set_host_assert_info(WMTDRV_TYPE_WMT, 30, 1);

	if (stp_dbg_set_fw_info(pbuf, len, STP_HOST_TRIGGER_COLLECT_FTRACE))
		return -1;

	if (g_core_dump) {
		osal_strncpy(&g_core_dump->info[0], pbuf, len);
#if IS_ENABLED(CONFIG_MTK_AEE_AED)
		aed_combo_exception(NULL, 0, (const PINT32)pbuf, len, (const PINT8)g_core_dump->info);
#endif
	} else {
		STP_DBG_PR_INFO("g_core_dump is not initialized\n");
#if IS_ENABLED(CONFIG_MTK_AEE_AED)
		aed_combo_exception(NULL, 0, (const PINT32)pbuf, len, (const PINT8)pbuf);
#endif
	}

	return 0;
}

#if BTIF_RXD_BE_BLOCKED_DETECT
MTK_WCN_BOOL stp_dbg_is_btif_rxd_be_blocked(VOID)
{
	MTK_WCN_BOOL flag = MTK_WCN_BOOL_FALSE;

	if (mtk_btif_rxd_be_blocked_flag_get())
		flag = MTK_WCN_BOOL_TRUE;
	return flag;
}
#endif

static _osal_inline_ INT32 stp_dbg_gzip_compressor(PVOID worker, PUINT8 in_buf, INT32 in_sz, PUINT8 out_buf,
				 PINT32 out_sz, INT32 finish)
{
	INT32 ret = 0;
	z_stream *stream = NULL;
	INT32 tmp = *out_sz;

	STP_DBG_PR_DBG("before compressor:buf 0x%zx, size %d, avalible buf: 0x%zx, size %d\n",
			(SIZE_T) in_buf, in_sz, (SIZE_T) out_buf, tmp);

	stream = (z_stream *) worker;
	if (!stream) {
		STP_DBG_PR_ERR("invalid workspace!\n");
		return -1;
	}

	if (in_sz > 0) {
#if 0
		ret = zlib_deflateReset(stream);
		if (ret != Z_OK) {
			STP_DBG_PR_ERR("reset failed!\n");
			return -2;
		}
#endif
		stream->next_in = in_buf;
		stream->avail_in = in_sz;
		stream->next_out = out_buf;
		stream->avail_out = tmp;

		zlib_deflate(stream, Z_FULL_FLUSH);

		if (finish) {
			while (1) {
				INT32 val = zlib_deflate(stream, Z_FINISH);

				if (val == Z_OK)
					continue;
				else if (val == Z_STREAM_END)
					break;
				STP_DBG_PR_ERR("finish operation failed %d\n", val);
				return -3;
			}
		}
		*out_sz = tmp - stream->avail_out;
	}

	STP_DBG_PR_DBG("after compressor,avalible buf: 0x%zx, compress rate %d -> %d\n",
			(SIZE_T) out_buf, in_sz, *out_sz);

	return ret;
}

/* stp_dbg_compressor_init - create a compressor and do init
 * @ name - compressor's name
 * @ L1_buf_sz - L1 buffer size
 * @ L2_buf_sz - L2 buffer size
 *
 * Retunr object's pointer if success, else NULL
 */
static _osal_inline_ P_WCN_COMPRESSOR_T stp_dbg_compressor_init(PUINT8 name, INT32 L1_buf_sz,
		INT32 L2_buf_sz)
{
	INT32 ret = 0;
	z_stream *pstream = NULL;
	P_WCN_COMPRESSOR_T compress = NULL;

	compress = (P_WCN_COMPRESSOR_T) osal_malloc(sizeof(WCN_COMPRESSOR_T));
	if (!compress) {
		STP_DBG_PR_ERR("alloc compressor failed!\n");
		goto fail;
	}

	osal_memset(compress, 0, sizeof(WCN_COMPRESSOR_T));
	osal_memcpy(compress->name, name, STP_OJB_NAME_SZ);

	compress->f_compress_en = 0;
	compress->compress_type = GZIP;

	if (compress->compress_type == GZIP) {
		compress->worker = osal_malloc(sizeof(z_stream));
		if (!compress->worker) {
			STP_DBG_PR_ERR("alloc stream failed!\n");
			goto fail;
		}
		pstream = (z_stream *) compress->worker;

		pstream->workspace = osal_malloc(zlib_deflate_workspacesize(MAX_WBITS, MAX_MEM_LEVEL));
		if (!pstream->workspace) {
			STP_DBG_PR_ERR("alloc workspace failed!\n");
			goto fail;
		}
		ret = zlib_deflateInit2(pstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
				  DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
		if (ret != Z_OK) {
			STP_DBG_PR_INFO("[%s::%d] zlib_deflateInit2 failed!\n", __func__, __LINE__);
			goto fail;
		}
	}

	compress->handler = stp_dbg_gzip_compressor;
	compress->L1_buf_sz = L1_buf_sz;
	compress->L2_buf_sz = L2_buf_sz;
	compress->L1_pos = 0;
	compress->L2_pos = 0;
	compress->uncomp_size = 0;
	compress->crc32 = 0xffffffffUL;

	compress->L1_buf = osal_malloc(compress->L1_buf_sz);
	if (!compress->L1_buf) {
		STP_DBG_PR_ERR("alloc %d bytes for L1 buf failed!\n", compress->L1_buf_sz);
		goto fail;
	}

	compress->L2_buf = osal_malloc(compress->L2_buf_sz);
	if (!compress->L2_buf) {
		STP_DBG_PR_ERR("alloc %d bytes for L2 buf failed!\n", compress->L2_buf_sz);
		goto fail;
	}

	STP_DBG_PR_INFO("create compressor OK! L1 %d bytes, L2 %d bytes\n", L1_buf_sz, L2_buf_sz);
	return compress;

fail:
	if (compress) {
		if (compress->L2_buf) {
			osal_free(compress->L2_buf);
			compress->L2_buf = NULL;
		}

		if (compress->L1_buf) {
			osal_free(compress->L1_buf);
			compress->L1_buf = NULL;
		}

		if (compress->worker) {
			pstream = (z_stream *) compress->worker;
			if ((compress->compress_type == GZIP) && pstream->workspace) {
				zlib_deflateEnd(pstream);
				osal_free(pstream->workspace);
			}
			osal_free(compress->worker);
			compress->worker = NULL;
		}

		if (compress->worker) {
			osal_free(compress->worker);
			compress->worker = NULL;
		}

		osal_free(compress);
		compress = NULL;
	}

	STP_DBG_PR_ERR("init failed!\n");

	return NULL;
}

/* stp_dbg_compressor_deinit - distroy a compressor
 * @ cprs - compressor's pointer
 *
 * Retunr 0 if success, else NULL
 */
static _osal_inline_ INT32 stp_dbg_compressor_deinit(P_WCN_COMPRESSOR_T cprs)
{
	z_stream *pstream = NULL;

	if (cprs) {
		if (cprs->L2_buf) {
			osal_free(cprs->L2_buf);
			cprs->L2_buf = NULL;
		}

		if (cprs->L1_buf) {
			osal_free(cprs->L1_buf);
			cprs->L1_buf = NULL;
		}

		if (cprs->worker) {
			pstream = (z_stream *) cprs->worker;
			if ((cprs->compress_type == GZIP) && pstream->workspace) {
				zlib_deflateEnd(pstream);
				osal_free(pstream->workspace);
			}
			osal_free(cprs->worker);
			cprs->worker = NULL;
		}

		cprs->handler = NULL;

		osal_free(cprs);
	}

	STP_DBG_PR_INFO("destroy OK\n");

	return 0;
}

/* stp_dbg_compressor_in - put in a raw data, and compress L1 buffer if need
 * @ cprs - compressor's pointer
 * @ buf - raw data buffer
 * @ len - raw data length
 * @ is_iobuf - is buf a pointer to EMI? 1: yes, 0: no
 * @ finish - core dump finish or not, 1: finished; 0: not finish
 *
 * Retunr 0 if success, else NULL
 */
static _osal_inline_ INT32 stp_dbg_compressor_in(P_WCN_COMPRESSOR_T cprs, PUINT8 buf, INT32 len,
		INT32 is_iobuf, INT32 finish)
{
	INT32 tmp_len = 0;
	INT32 ret = 0;

	if (!cprs) {
		STP_DBG_PR_ERR("invalid para!\n");
		return -1;
	}

	cprs->uncomp_size += len;

	/* check L1 buf valid space */
	if (len > (cprs->L1_buf_sz - cprs->L1_pos)) {
		STP_DBG_PR_DBG("L1 buffer full\n");

		if (cprs->f_compress_en && cprs->handler) {
			/* need compress */
			/* compress L1 buffer, and put result to L2 buffer */
			tmp_len = cprs->L2_buf_sz - cprs->L2_pos;
			ret =
			    cprs->handler(cprs->worker, cprs->L1_buf, cprs->L1_pos,
					    &cprs->L2_buf[cprs->L2_pos], &tmp_len, finish);
			if (!ret) {
				cprs->crc32 = (crc32(cprs->crc32, cprs->L1_buf, cprs->L1_pos));
				cprs->L2_pos += tmp_len;
				if (cprs->L2_pos >= cprs->L2_buf_sz)
					STP_DBG_PR_ERR("coredump size too large(%d), L2 buf overflow\n",
					cprs->L2_pos);

				if (finish) {
					/* Add 8 byte suffix
					 * ===
					 * 32 bits UNCOMPRESS SIZE
					 * 32 bits CRC
					 */
					*(uint32_t *) (&cprs->L2_buf[cprs->L2_pos]) =
						(cprs->crc32 ^ 0xffffffffUL);
					*(uint32_t *) (&cprs->L2_buf[cprs->L2_pos + 4]) = cprs->uncomp_size;
					cprs->L2_pos += 8;
				}
				STP_DBG_PR_DBG("compress OK!\n");
			} else
				STP_DBG_PR_ERR("compress error!\n");
		} else {
			/* no need compress */
			/* Flush L1 buffer to L2 buffer */
			STP_DBG_PR_INFO("No need do compress, Put to L2 buf\n");

			tmp_len = cprs->L2_buf_sz - cprs->L2_pos;
			tmp_len = (cprs->L1_pos > tmp_len) ? tmp_len : cprs->L1_pos;
			osal_memcpy(&cprs->L2_buf[cprs->L2_pos], cprs->L1_buf, tmp_len);
			cprs->L2_pos += tmp_len;
		}

		/* reset L1 buf pos */
		cprs->L1_pos = 0;

		/* put curren data to L1 buf */
		if (len > cprs->L1_buf_sz) {
			STP_DBG_PR_ERR("len=%d, too long err!\n", len);
		} else {
			STP_DBG_PR_DBG("L1 Flushed, and Put %d bytes to L1 buf\n", len);
			if (is_iobuf)
				osal_memcpy_fromio(&cprs->L1_buf[cprs->L1_pos], buf, len);
			else
				osal_memcpy(&cprs->L1_buf[cprs->L1_pos], buf, len);
			cprs->L1_pos += len;
		}
	} else {
		/* put to L1 buffer */
		STP_DBG_PR_DBG("Put %d bytes to L1 buf\n", len);
		if (is_iobuf)
			osal_memcpy_fromio(&cprs->L1_buf[cprs->L1_pos], buf, len);
		else
			osal_memcpy(&cprs->L1_buf[cprs->L1_pos], buf, len);
		cprs->L1_pos += len;
	}

	return ret;
}

/* stp_dbg_compressor_out - get the result data from L2 buffer
 * @ cprs - compressor's pointer
 * @ pbuf - point to L2 buffer
 * @ plen - out len
 *
 * Retunr 0 if success, else NULL
 */
static _osal_inline_ INT32 stp_dbg_compressor_out(P_WCN_COMPRESSOR_T cprs, PPUINT8 pbuf, PINT32 plen)
{
	INT32 ret = 0;
	INT32 tmp_len = 0;

	if ((!cprs) || (!pbuf) || (!plen)) {
		STP_DBG_PR_ERR("invalid para!\n");
		return -1;
	}
	/* check if there's L1 data need flush to L2 buffer */
	if (cprs->L1_pos > 0) {
		tmp_len = cprs->L2_buf_sz - cprs->L2_pos;

		if (cprs->f_compress_en && cprs->handler) {
			/* need compress */
			ret =
			    cprs->handler(cprs->worker, cprs->L1_buf, cprs->L1_pos,
					    &cprs->L2_buf[cprs->L2_pos], &tmp_len, 1);

			if (!ret) {
				cprs->crc32 = (crc32(cprs->crc32, cprs->L1_buf, cprs->L1_pos));
				cprs->L2_pos += tmp_len;

				/* Add 8 byte suffix
				 * ===
				 * 32 bits UNCOMPRESS SIZE
				 * 32 bits CRC
				 */
				*(uint32_t *) (&cprs->L2_buf[cprs->L2_pos]) = (cprs->crc32 ^ 0xffffffffUL);
				*(uint32_t *) (&cprs->L2_buf[cprs->L2_pos + 4]) = cprs->uncomp_size;
				cprs->L2_pos += 8;

				STP_DBG_PR_INFO("compress OK!\n");
			} else {
				STP_DBG_PR_ERR("compress error!\n");
			}
		} else {
			/* no need compress */
			tmp_len = (cprs->L1_pos > tmp_len) ? tmp_len : cprs->L1_pos;
			osal_memcpy(&cprs->L2_buf[cprs->L2_pos], cprs->L1_buf, tmp_len);
			cprs->L2_pos += tmp_len;
		}

		cprs->L1_pos = 0;
	}

	*pbuf = cprs->L2_buf;
	*plen = cprs->L2_pos;

	STP_DBG_PR_INFO("0x%zx, len %d, l2_buf_remain %d\n", (SIZE_T)*pbuf, *plen, cprs->L2_buf_sz - cprs->L2_pos);

#if 1
	ret = zlib_deflateReset((z_stream *) cprs->worker);
	if (ret != Z_OK) {
		STP_DBG_PR_ERR("reset failed!\n");
		return -2;
	}
#endif
	return 0;
}

/* stp_dbg_compressor_reset - reset compressor
 * @ cprs - compressor's pointer
 * @ enable - enable/disable compress
 * @ type - compress algorithm
 *
 * Retunr 0 if success, else NULL
 */
static _osal_inline_ INT32 stp_dbg_compressor_reset(P_WCN_COMPRESSOR_T cprs, UINT8 enable,
		WCN_COMPRESS_ALG_T type)
{
	if (!cprs) {
		STP_DBG_PR_ERR("invalid para!\n");
		return -1;
	}

	cprs->f_compress_en = enable;
	/* cprs->f_compress_en = 0; // disable compress for test */
	cprs->compress_type = type;
	cprs->L1_pos = 0;
	cprs->L2_pos = 0;
	cprs->uncomp_size = 0;
	cprs->crc32 = 0xffffffffUL;

	/* zlib_deflateEnd((z_stream*)cprs->worker); */

	STP_DBG_PR_INFO("OK! compress algorithm %d\n", type);

	return 0;
}

#if 0
static _osal_inline_ VOID stp_dbg_dump_data(PUINT8 pBuf, PINT8 title, INT32 len)
{
	INT32 idx = 0;
	UINT8 str[240];
	PUINT8 p_str;

	p_str = &str[0];
	pr_debug(" %s-len:%d\n", title, len);
	for (idx = 0; idx < len; idx++, pBuf++) {
		sprintf(p_str, "%02x ", *pBuf);
		p_str += 3;
		if (15 == (idx % 16)) {
			sprintf(p_str, "--end\n");
			*(p_str + 6) = '\0';
			pr_debug("%s", str);
			p_str = 0;
		}
	}
	if (len % 16) {
		sprintf(p_str, "--end\n");
		*(p_str + 6) = '\0';
		pr_debug("%s", str);
	}
}
#endif
static VOID stp_dbg_dump_data(PUINT8 pBuf, PINT8 title, INT32 len)
{
	INT32 k = 0;
	char str[240] = {""};
	char buf_str[32] = {""};

	pr_warn(" %s-len:%d\n", title, len);
	/* pr_warn("    ", title, len); */
	for (k = 0; k < len; k++) {
		if (strlen(str) < 200) {
			if (snprintf(buf_str, sizeof(buf_str), "0x%02x ", pBuf[k]) > 0)
				strncat(str, buf_str, strlen(buf_str));
		} else {
			pr_warn("More than 200 of the data is too much\n");
			break;
		}
	}
	strncat(str, "--end\n", strlen("--end\n"));
	pr_warn("%s", str);
}


INT32 stp_dbg_enable(MTKSTP_DBG_T *stp_dbg)
{
	ULONG flags;

	spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);
	stp_dbg->pkt_trace_no = 0;
	stp_dbg->is_enable = 1;
	spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);

	return 0;
}

INT32 stp_dbg_disable(MTKSTP_DBG_T *stp_dbg)
{
	ULONG flags;

	spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);
	stp_dbg->pkt_trace_no = 0;
	memset(stp_dbg->logsys, 0, sizeof(MTKSTP_LOG_SYS_T));
	stp_dbg->is_enable = 0;
	spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);

	return 0;
}

static PINT8 stp_get_dbg_type_string(const PINT8 *pType, UINT32 type)
{
	PINT8 info_task_type = "<DBG>";

	if (!pType)
		return NULL;

	if ((mtk_wcn_stp_is_support_gpsl5() == 0) && (type == INFO_TASK_INDX))
		return info_task_type;
	else
		return pType[type];
}

static _osal_inline_ INT32 stp_dbg_dmp_in(MTKSTP_DBG_T *stp_dbg, PINT8 buf, INT32 len)
{
	ULONG flags;
	STP_DBG_HDR_T *pHdr = NULL;
	PINT8 pBuf = NULL;
	UINT32 length = 0;
	const PINT8 *pType = NULL;

	pType = wmt_detect_get_chip_type() == WMT_CHIP_TYPE_COMBO ?
		comboStpDbgType : socStpDbgType;

	spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);

	stp_dbg->logsys->queue[stp_dbg->logsys->in].id = 0;
	stp_dbg->logsys->queue[stp_dbg->logsys->in].len = len;
	memset(&(stp_dbg->logsys->queue[stp_dbg->logsys->in].buffer[0]),
	       0, ((len >= STP_DBG_LOG_ENTRY_SZ) ? (STP_DBG_LOG_ENTRY_SZ) : (len)));
	memcpy(&(stp_dbg->logsys->queue[stp_dbg->logsys->in].buffer[0]),
	       buf, ((len >= STP_DBG_LOG_ENTRY_SZ) ? (STP_DBG_LOG_ENTRY_SZ) : (len)));
	stp_dbg->logsys->size++;
	stp_dbg->logsys->size = (stp_dbg->logsys->size > STP_DBG_LOG_ENTRY_NUM) ?
				STP_DBG_LOG_ENTRY_NUM : stp_dbg->logsys->size;
	if (gStpDbgLogOut != 0) {
		pHdr = (STP_DBG_HDR_T *) &(stp_dbg->logsys->queue[stp_dbg->logsys->in].buffer[0]);
		pBuf = (PINT8)&(stp_dbg->logsys->queue[stp_dbg->logsys->in].buffer[0]) +
			sizeof(STP_DBG_HDR_T);
		length = stp_dbg->logsys->queue[stp_dbg->logsys->in].len - sizeof(STP_DBG_HDR_T);
		pr_info("STP-DBG:%d.%ds, %s:pT%sn(%d)l(%d)s(%d)a(%d)\n",
			pHdr->sec,
			pHdr->usec,
			pHdr->dir == PKT_DIR_TX ? "Tx" : "Rx",
			stp_get_dbg_type_string(pType, pHdr->type),
			pHdr->no, pHdr->len, pHdr->seq, pHdr->ack);

		if (length > 0)
			stp_dbg_dump_data(pBuf, pHdr->dir == PKT_DIR_TX ? "Tx" : "Rx", length);
	}
	stp_dbg->logsys->in =
	    (stp_dbg->logsys->in >= (STP_DBG_LOG_ENTRY_NUM - 1)) ? (0) : (stp_dbg->logsys->in + 1);
	STP_DBG_PR_DBG("logsys size = %d, in = %d\n", stp_dbg->logsys->size, stp_dbg->logsys->in);

	spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);

	return 0;
}

static _osal_inline_ INT32 stp_dbg_notify_btm_dmp_wq(MTKSTP_DBG_T *stp_dbg)
{
	INT32 retval = 0;

/* #ifndef CONFIG_LOG_STP_INTERNAL */
	if (stp_dbg->btm != NULL)
		retval += stp_btm_notify_wmt_dmp_wq((MTKSTP_BTM_T *) stp_dbg->btm);
/* #endif */

	return retval;
}

static VOID stp_dbg_dmp_print_work(struct work_struct *work)
{
	MTKSTP_LOG_SYS_T *logsys = container_of(work, MTKSTP_LOG_SYS_T, dump_work);
	INT32 dumpSize = logsys->dump_size;
	MTKSTP_LOG_ENTRY_T *queue = logsys->dump_queue;
	INT32 i;
	PINT8 pBuf = NULL;
	INT32 len = 0;
	STP_DBG_HDR_T *pHdr = NULL;
	const PINT8 *pType = NULL;

	if (queue == NULL || queue == (MTKSTP_LOG_ENTRY_T *)STP_MAGIC_NUM)
		return;

	pType = wmt_detect_get_chip_type() == WMT_CHIP_TYPE_COMBO ?
		comboStpDbgType : socStpDbgType;

	for (i = 0; i < dumpSize; i++) {
		pHdr = (STP_DBG_HDR_T *) &(queue[i].buffer[0]);
		pBuf = &(queue[i].buffer[0]) + sizeof(STP_DBG_HDR_T);
		len = queue[i].len - sizeof(STP_DBG_HDR_T);
		len = len > STP_PKT_SZ ? STP_PKT_SZ : len;
		pr_info("STP-DBG:%d.%ds, %s:pT%sn(%d)l(%d)s(%d)a(%d), time[%llu.%06lu]\n",
			pHdr->sec,
			pHdr->usec,
			pHdr->dir == PKT_DIR_TX ? "Tx" : "Rx",
			stp_get_dbg_type_string(pType, pHdr->type),
			pHdr->no, pHdr->len, pHdr->seq,
			pHdr->ack, pHdr->l_sec, pHdr->l_nsec);

		if (len > 0)
			stp_dbg_dump_data(pBuf, pHdr->dir == PKT_DIR_TX ? "Tx" : "Rx", len);

	}
	vfree(queue);
	logsys->dump_queue = NULL;
}

INT32 stp_dbg_dmp_print(MTKSTP_DBG_T *stp_dbg)
{
#define MAX_DMP_NUM 80
	ULONG flags;
	UINT32 dumpSize = 0;
	UINT32 inIndex = 0;
	UINT32 outIndex = 0;
	MTKSTP_LOG_ENTRY_T *dump_queue = NULL;
	MTKSTP_LOG_ENTRY_T *queue = stp_dbg->logsys->queue;

	spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);
	if (stp_dbg->logsys->dump_queue != NULL) {
		spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);
		return 0;
	}

	stp_dbg->logsys->dump_queue = (MTKSTP_LOG_ENTRY_T *)STP_MAGIC_NUM;
	spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);

	/* allocate memory may take long time, thus allocate it before get lock */
	dump_queue = vmalloc(sizeof(MTKSTP_LOG_ENTRY_T) * MAX_DMP_NUM);
	if (dump_queue == NULL) {
		stp_dbg->logsys->dump_queue = NULL;
		pr_info("fail to allocate memory");
		return -1;
	}

	if (spin_trylock_irqsave(&(stp_dbg->logsys->lock), flags) == 0) {
		stp_dbg->logsys->dump_queue = NULL;
		vfree(dump_queue);
		pr_info("fail to get lock");
		return -1;
	}
	/* Not to dequeue from loging system */
	inIndex = stp_dbg->logsys->in;
	dumpSize = stp_dbg->logsys->size;

	/* chance is little but still needs to check */
	if (dumpSize == 0) {
		stp_dbg->logsys->dump_queue = NULL;
		spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);
		vfree(dump_queue);
		return 0;
	}

	if (dumpSize == STP_DBG_LOG_ENTRY_NUM)
		outIndex = inIndex;
	else
		outIndex = ((inIndex + STP_DBG_LOG_ENTRY_NUM) - dumpSize) % STP_DBG_LOG_ENTRY_NUM;

	if (dumpSize > MAX_DMP_NUM) {
		outIndex += (dumpSize - MAX_DMP_NUM);
		outIndex %= STP_DBG_LOG_ENTRY_NUM;
		dumpSize = MAX_DMP_NUM;
	}

	stp_dbg->logsys->dump_queue = dump_queue;
	stp_dbg->logsys->dump_size = dumpSize;

	/* copy content of stp_dbg->logsys->queue out, don't print log while holding */
	/* spinlock to prevent blocking other process */
	if (outIndex + dumpSize > STP_DBG_LOG_ENTRY_NUM) {
		UINT32 tailNum = STP_DBG_LOG_ENTRY_NUM  - outIndex;

		osal_memcpy(dump_queue, &(queue[outIndex]), sizeof(MTKSTP_LOG_ENTRY_T) * tailNum);
		osal_memcpy(dump_queue + tailNum, &(queue[0]), sizeof(MTKSTP_LOG_ENTRY_T) *
			(dumpSize - tailNum));
	} else {
		osal_memcpy(dump_queue, &(queue[outIndex]), sizeof(MTKSTP_LOG_ENTRY_T) * dumpSize);
	}

	spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);
	STP_DBG_PR_INFO("loged packet size = %d, in(%d), out(%d)\n", dumpSize, inIndex, outIndex);
	schedule_work(&(stp_dbg->logsys->dump_work));
	return 0;
}

INT32 stp_dbg_dmp_out(MTKSTP_DBG_T *stp_dbg, PINT8 buf, PINT32 len)
{
	ULONG flags;
	INT32 remaining = 0;
	*len = 0;
	spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);

	if (stp_dbg->logsys->size > 0) {
		if (stp_dbg->logsys->queue[stp_dbg->logsys->out].len >= STP_DBG_LOG_ENTRY_SZ)
			stp_dbg->logsys->queue[stp_dbg->logsys->out].len = STP_DBG_LOG_ENTRY_SZ - 1;
		memcpy(buf, &(stp_dbg->logsys->queue[stp_dbg->logsys->out].buffer[0]),
		       stp_dbg->logsys->queue[stp_dbg->logsys->out].len);

		(*len) = stp_dbg->logsys->queue[stp_dbg->logsys->out].len;
		stp_dbg->logsys->out =
		    (stp_dbg->logsys->out >= (STP_DBG_LOG_ENTRY_NUM - 1)) ?
		    (0) : (stp_dbg->logsys->out + 1);
		stp_dbg->logsys->size--;

		STP_DBG_PR_DBG("logsys size = %d, out = %d\n", stp_dbg->logsys->size,
				stp_dbg->logsys->out);
	} else
		STP_DBG_PR_LOUD("logsys EMPTY!\n");

	remaining = (stp_dbg->logsys->size == 0) ? (0) : (1);

	spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);

	return remaining;
}

INT32 stp_dbg_dmp_out_ex(PINT8 buf, PINT32 len)
{
	return stp_dbg_dmp_out(g_stp_dbg, buf, len);
}

INT32 stp_dbg_dmp_append(MTKSTP_DBG_T *stp_dbg, PUINT8 pBuf, INT32 max_len)
{
	PUINT8 p = NULL;
	UINT32 l = 0;
	UINT32 i = 0;
	INT32 j = 0;
	ULONG flags;
	UINT32 len = 0;
	UINT32 dumpSize = 0;
	STP_DBG_HDR_T *pHdr = NULL;
	const PINT8 *pType = NULL;

	if (!pBuf || max_len < 8) { /* 8: length of "<!---->\n" */
		STP_DBG_PR_WARN("invalid param, pBuf:%p, max_len:%d\n", pBuf, max_len);
		return 0;
	}

	pType = wmt_detect_get_chip_type() == WMT_CHIP_TYPE_COMBO ?
		comboStpDbgType : socStpDbgType;
	spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);
	/* Not to dequeue from loging system */
	dumpSize = stp_dbg->logsys->size;
	j = stp_dbg->logsys->in;

	/* format <!-- XXX -->*/
	len += osal_sprintf(pBuf, "<!--\n");

	while (dumpSize > 0) {
		j--;
		if (j < 0)
			j += STP_DBG_LOG_ENTRY_NUM;

		l = stp_dbg->logsys->queue[j].len - sizeof(STP_DBG_HDR_T);
		l = l > STP_PKT_SZ ? STP_PKT_SZ : l;

		/* format "\t9999999.999999s, Tx:<STP>n(999999)l(1024)s(7)a(7)	xx yy zz\n"
		 * need to consider "-->\n"
		*/
		if ((len + 53 + 3 * l + 4) > max_len)
			break;

		pHdr = (STP_DBG_HDR_T *) &(stp_dbg->logsys->queue[j].buffer[0]);
		p = (PUINT8)pHdr + sizeof(STP_DBG_HDR_T);

		len += osal_sprintf(pBuf + len, "\t%llu.%06lus, %s:pT%sn(%d)l(%4d)s(%d)a(%d)\t",
				    pHdr->l_sec, pHdr->l_nsec,
				    pHdr->dir == PKT_DIR_TX ? "Tx" : "Rx",
				    stp_get_dbg_type_string(pType, pHdr->type),
				    pHdr->no, pHdr->len, pHdr->seq,
				    pHdr->ack);

		for (i = 0; i < l; i++, p++)
			len += osal_sprintf(pBuf + len, " %02x", *p, 3);

		pBuf[len] = '\n';
		len += 1;

		dumpSize--;
	}

	len += osal_sprintf(pBuf + len, "-->\n");

	spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);

	return len;
}

static _osal_inline_ INT32 stp_dbg_get_avl_entry_num(MTKSTP_DBG_T *stp_dbg)
{
	if (stp_dbg->logsys->size == 0)
		return STP_DBG_LOG_ENTRY_NUM;
	else
		return (stp_dbg->logsys->in > stp_dbg->logsys->out) ?
		    (STP_DBG_LOG_ENTRY_NUM - stp_dbg->logsys->in + stp_dbg->logsys->out) :
		    (stp_dbg->logsys->out - stp_dbg->logsys->in);
}

static _osal_inline_ INT32 stp_dbg_fill_hdr(STP_DBG_HDR_T *hdr, INT32 type, INT32 ack, INT32 seq,
			      INT32 crc, INT32 dir, INT32 len, INT32 dbg_type)
{

	struct timeval now;
	UINT64 ts;
	ULONG nsec;

	if (!hdr) {
		STP_DBG_PR_ERR("function invalid\n");
		return -EINVAL;
	}

	osal_do_gettimeofday(&now);
	osal_get_local_time(&ts, &nsec);
	hdr->last_dbg_type = gStpDbgDumpType;
	gStpDbgDumpType = dbg_type;
	hdr->dbg_type = dbg_type;
	hdr->ack = ack;
	hdr->seq = seq;
	hdr->sec = now.tv_sec;
	hdr->usec = now.tv_usec;
	hdr->crc = crc;
	hdr->dir = dir;	/* rx */
	hdr->dmy = 0xffffffff;
	hdr->len = len;
	hdr->type = type;
	hdr->l_sec = ts;
	hdr->l_nsec = nsec;
	return 0;
}

static _osal_inline_ INT32 stp_dbg_add_pkt(MTKSTP_DBG_T *stp_dbg, STP_DBG_HDR_T *hdr, const PUINT8 body)
{
	/* fix the frame size large issues. */
	static STP_PACKET_T stp_pkt;
	UINT32 hdr_sz = sizeof(struct stp_dbg_pkt_hdr);
	UINT32 body_sz = 0;
	ULONG flags;
	UINT32 avl_num;

	if (hdr->dbg_type == STP_DBG_PKT)
		body_sz = (hdr->len <= STP_PKT_SZ) ? (hdr->len) : (STP_PKT_SZ);
	else
		body_sz = (hdr->len <= STP_DMP_SZ) ? (hdr->len) : (STP_DMP_SZ);

	hdr->no = stp_dbg->pkt_trace_no++;
	memcpy((PUINT8) &stp_pkt.hdr, (PUINT8) hdr, hdr_sz);
	if (body != NULL)
		memcpy((PUINT8) &stp_pkt.raw[0], body, body_sz);

	if (hdr->dbg_type == STP_DBG_FW_DMP) {
		if (hdr->last_dbg_type != STP_DBG_FW_DMP) {

			STP_DBG_PR_INFO
			    ("reset stp_dbg logsys when queue fw coredump package(%d)\n",
			     hdr->last_dbg_type);
			STP_DBG_PR_INFO("dump 1st fw coredump package len(%d) for confirming\n",
					  hdr->len);
			spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);
			stp_dbg->logsys->in = 0;
			stp_dbg->logsys->out = 0;
			stp_dbg->logsys->size = 0;
			spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);
		} else {
			avl_num = stp_dbg_get_avl_entry_num(stp_dbg);

			if (!avl_num)
				STP_DBG_PR_ERR("there is no avl entry stp_dbg logsys!!!\n");
		}
	}
	stp_dbg_dmp_in(stp_dbg, (PINT8) &stp_pkt, hdr_sz + body_sz);
	/* Only FW DMP MSG should inform BTM-CORE to dump packet to native process */
	if (hdr->dbg_type == STP_DBG_FW_DMP)
		stp_dbg_notify_btm_dmp_wq(stp_dbg);

	return 0;
}

INT32 stp_dbg_log_pkt(MTKSTP_DBG_T *stp_dbg, INT32 dbg_type,
		      INT32 type, INT32 ack_no, INT32 seq_no, INT32 crc, INT32 dir, INT32 len,
		      const PUINT8 body)
{
	STP_DBG_HDR_T hdr;

	osal_bug_on(!stp_dbg);

	if (!stp_dbg)
		return -1;

	if (stp_dbg->is_enable == 0) {
		/*dbg is disable,and not to log */
	} else {
		hdr.no = 0;
		hdr.chs = 0;
		stp_dbg_fill_hdr(&hdr,
				 (INT32) type,
				 (INT32) ack_no,
				 (INT32) seq_no, (INT32) crc, (INT32) dir, (INT32) len,
				 (INT32) dbg_type);

		stp_dbg_add_pkt(stp_dbg, &hdr, body);
	}

	return 0;
}

INT32 stp_dbg_log_ctrl(UINT32 on)
{
	if (on != 0) {
		gStpDbgLogOut = 1;
		pr_warn("STP-DBG: enable pkt log dump out.\n");
	} else {
		gStpDbgLogOut = 0;
		pr_warn("STP-DBG: disable pkt log dump out.\n");
	}

	return 0;
}

VOID stp_dbg_nl_init(VOID)
{
#if 0
	if (genl_register_family(&stp_dbg_gnl_family) != 0) {
		STP_DBG_PR_ERR("%s(): GE_NELINK family registration fail\n", __func__);
	} else {
		if (genl_register_ops(&stp_dbg_gnl_family, &stp_dbg_gnl_ops_bind) != 0)
			STP_DBG_PR_ERR("%s(): BIND operation registration fail\n", __func__);

		if (genl_register_ops(&stp_dbg_gnl_family, &stp_dbg_gnl_ops_reset) != 0)
			STP_DBG_PR_ERR("%s(): RESET operation registration fail\n", __func__);

	}
#endif
	osal_sleepable_lock_init(&g_dbg_nl_lock);
	if (genl_register_family(&stp_dbg_gnl_family) != 0)
		STP_DBG_PR_ERR("%s(): GE_NELINK family registration fail\n", __func__);
}

VOID stp_dbg_nl_deinit(VOID)
{
	int i;

	num_bind_process = 0;
	for (i = 0; i < MAX_BIND_PROCESS; i++)
		bind_pid[i] = 0;
	genl_unregister_family(&stp_dbg_gnl_family);
	osal_sleepable_lock_deinit(&g_dbg_nl_lock);
}

static INT32 stp_dbg_nl_bind(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *na = NULL;
	PINT8 mydata;
	INT32 i;

	if (info == NULL)
		goto out;

	STP_DBG_PR_INFO("%s():->\n", __func__);

	na = info->attrs[STP_DBG_ATTR_MSG];

	if (na)
		mydata = (PINT8) nla_data(na);

	if (osal_lock_sleepable_lock(&g_dbg_nl_lock))
		return -1;

	for (i = 0; i < MAX_BIND_PROCESS; i++) {
		if (bind_pid[i] == 0) {
			bind_pid[i] = info->snd_portid;
			num_bind_process++;
			STP_DBG_PR_INFO("%s():-> pid  = %d\n", __func__, info->snd_portid);
			break;
		}
	}

	if (i == MAX_BIND_PROCESS) {
		STP_DBG_PR_ERR("%s(): exceeding binding limit %d\n", __func__, MAX_BIND_PROCESS);
		bind_pid[0] = info->snd_portid;
	}

	osal_unlock_sleepable_lock(&g_dbg_nl_lock);

out:
	return 0;
}

static INT32 stp_dbg_nl_reset(struct sk_buff *skb, struct genl_info *info)
{
	STP_DBG_PR_ERR("%s(): should not be invoked\n", __func__);

	return 0;
}

INT32 stp_dbg_nl_send(PINT8 aucMsg, UINT8 cmd, INT32 len)
{
	struct sk_buff *skb = NULL;
	PVOID msg_head = NULL;
	INT32 rc = -1;
	INT32 i, j;
	INT32 ret = 0;
	INT32 killed_num = 0;

	if (num_bind_process == 0) {
		/* no listening process */
		STP_DBG_PR_ERR("%s(): the process is not invoked\n", __func__);
		return 0;
	}

	ret = stp_dbg_core_dump_nl(g_core_dump, aucMsg, len);
	if (ret < 0)
		return ret;
	if (ret == 32)
		return ret;

	ret = -1;
	for (i = 0; i < num_bind_process; i++) {
		if (bind_pid[i] == 0) {
			killed_num++;
			continue;
		}

		skb = genlmsg_new(2048, GFP_KERNEL);
		if (skb) {
			msg_head = genlmsg_put(skb, 0, stp_dbg_seqnum++, &stp_dbg_gnl_family, 0, cmd);
			if (msg_head == NULL) {
				nlmsg_free(skb);
				STP_DBG_PR_ERR("%s(): genlmsg_put fail...\n", __func__);
				return -1;
			}

			rc = nla_put(skb, STP_DBG_ATTR_MSG, len, aucMsg);
			if (rc != 0) {
				nlmsg_free(skb);
				STP_DBG_PR_ERR("%s(): nla_put_string fail...: %d\n", __func__, rc);
				return rc;
			}

			/* finalize the message */
			genlmsg_end(skb, msg_head);

			/* sending message */
			rc = genlmsg_unicast(&init_net, skb, bind_pid[i]);
			if (rc != 0) {
				STP_DBG_PR_INFO("%s(): genlmsg_unicast fail...: %d pid: %d\n",
					__func__, rc, bind_pid[i]);
				if (rc == -ECONNREFUSED) {
					bind_pid[i] = 0;
					killed_num++;
				}
			} else {
				/* don't retry as long as at least one process receives data */
				ret = 0;
			}
		} else {
			STP_DBG_PR_ERR("%s(): genlmsg_new fail...\n", __func__);
		}
	}

	if (killed_num > 0) {
		if (osal_lock_sleepable_lock(&g_dbg_nl_lock)) {
			/* if fail to get lock, it is fine to update bind_pid[] later */
			return ret;
		}

		for (i = 0; i < num_bind_process - killed_num; i++) {
			if (bind_pid[i] == 0) {
				for (j = num_bind_process - 1; j > i; j--) {
					if (bind_pid[j] > 0) {
						bind_pid[i] = bind_pid[j];
						bind_pid[j] = 0;
					}
				}
			}
		}
		num_bind_process -= killed_num;
		osal_unlock_sleepable_lock(&g_dbg_nl_lock);
	}

	return ret;
}

INT32 stp_dbg_dump_send_retry_handler(PINT8 tmp, INT32 len)
{
	INT32 ret = 0;
	INT32 nl_retry = 0;

	if (tmp == NULL)
		return -1;

	ret = stp_dbg_nl_send(tmp, 2, len+5);
	while (ret) {
		nl_retry++;
		if (ret == 32) {
			STP_DBG_PR_ERR("**dump send timeout : %d**\n", ret);
			ret = 1;
			break;
		}
		if (nl_retry > 1000) {
			STP_DBG_PR_ERR("**dump send fails, and retry more than 1000: %d.**\n", ret);
			ret = 2;
			break;
		}
		STP_DBG_PR_WARN("**dump send fails, and retry again.**\n");
		osal_sleep_ms(3);
		ret = stp_dbg_nl_send(tmp, 2, len+5);
		if (!ret)
			STP_DBG_PR_DBG("****retry again ok!**\n");
	}

	return ret;
}

INT32 stp_dbg_aee_send(PUINT8 aucMsg, INT32 len, INT32 cmd)
{
#define KBYTES (1024*sizeof(char))
#ifndef LOG_STP_DEBUG_DISABLE
#define L1_BUF_SIZE (32*KBYTES)
#define PKT_MULTIPLIER 18
#else
#define L1_BUF_SIZE (4*KBYTES)
#define PKT_MULTIPLIER 1
#endif
	INT32 ret = 0;

	if (g_core_dump->count == 0) {
		g_core_dump->compressor = stp_dbg_compressor_init("core_dump_compressor",
								   L1_BUF_SIZE,
								   PKT_MULTIPLIER*g_core_dump->dmp_num*KBYTES);
		g_core_dump->count++;
		if (!g_core_dump->compressor) {
			STP_DBG_PR_ERR("create compressor failed!\n");
			stp_dbg_compressor_deinit(g_core_dump->compressor);
			return -1;
		}
	}
	/* buffered to compressor */
	ret = stp_dbg_core_dump_in(g_core_dump, aucMsg, len);
	if (ret == 1 && wmt_detect_get_chip_type() == WMT_CHIP_TYPE_COMBO)
		stp_dbg_core_dump_flush(0, MTK_WCN_BOOL_FALSE);

	return ret;
}

INT32 stp_dbg_dump_num(LONG dmp_num)
{
	g_core_dump->dmp_num = dmp_num;
	return 0;
}

static _osal_inline_ INT32 stp_dbg_parser_assert_str(PINT8 str, ENUM_ASSERT_INFO_PARSER_TYPE type)
{
#define WDT_INFO_HEAD "Watch Dog Timeout"
	PINT8 pStr = NULL;
	PINT8 pDtr = NULL;
	PINT8 pTemp = NULL;
	PINT8 pTemp2 = NULL;
	INT8 tempBuf[STP_ASSERT_TYPE_SIZE] = { 0 };
	UINT32 len = 0;
	LONG res;
	INT32 ret;
	INT32 remain_array_len = 0;

	PUINT8 parser_sub_string[] = {
		"<ASSERT> ",
		"id=",
		"isr=",
		"irq=",
		"rc="
	};

	if (!str) {
		STP_DBG_PR_ERR("NULL string source\n");
		return -1;
	}

	if (!g_stp_dbg_cpupcr) {
		STP_DBG_PR_ERR("NULL pointer\n");
		return -2;
	}

	pStr = str;
	STP_DBG_PR_DBG("source infor:%s\n", pStr);
	switch (type) {
	case STP_DBG_ASSERT_INFO:


		pDtr = osal_strstr(pStr, parser_sub_string[type]);
		if (pDtr != NULL) {
			pDtr += osal_strlen(parser_sub_string[type]);
			pTemp = osal_strchr(pDtr, ' ');
		} else {
			STP_DBG_PR_ERR("parser str is NULL,substring(%s)\n", parser_sub_string[type]);
			return -3;
		}

		if (pTemp == NULL) {
			STP_DBG_PR_ERR("delimiter( ) is not found,substring(%s)\n",
					 parser_sub_string[type]);
			return -4;
		}

		len = pTemp - pDtr;
		osal_memcpy(&g_stp_dbg_cpupcr->assert_info[0], "assert@", osal_strlen("assert@"));
		osal_memcpy(&g_stp_dbg_cpupcr->assert_info[osal_strlen("assert@")], pDtr, len);
		g_stp_dbg_cpupcr->assert_info[osal_strlen("assert@") + len] = '_';

		pTemp = osal_strchr(pDtr, '#');
		if (pTemp == NULL) {
			STP_DBG_PR_ERR("parser '#' is not find\n");
			return -5;
		}
		pTemp += 1;

		pTemp2 = osal_strchr(pTemp, ' ');
		if (pTemp2 == NULL) {
			STP_DBG_PR_ERR("parser ' ' is not find\n");
			pTemp2 = pTemp + 1;
		}
		remain_array_len = osal_array_size(g_stp_dbg_cpupcr->assert_info) - (osal_strlen("assert@") + len + 1);
		if (remain_array_len - 1 > pTemp2 - pTemp) {
			osal_memcpy(&g_stp_dbg_cpupcr->assert_info[osal_strlen("assert@") + len + 1], pTemp,
					pTemp2 - pTemp);
			g_stp_dbg_cpupcr->assert_info[osal_strlen("assert@") + len + 1 + pTemp2 - pTemp] = '\0';
		} else {
			osal_memcpy(&g_stp_dbg_cpupcr->assert_info[osal_strlen("assert@") + len + 1], pTemp,
					remain_array_len - 1);
			g_stp_dbg_cpupcr->assert_info[STP_ASSERT_INFO_SIZE - 1] = '\0';
		}
		STP_DBG_PR_INFO("assert info:%s\n", &g_stp_dbg_cpupcr->assert_info[0]);
		break;
	case STP_DBG_FW_TASK_ID:
		pDtr = osal_strstr(pStr, parser_sub_string[type]);
		if (pDtr != NULL) {
			pDtr += osal_strlen(parser_sub_string[type]);
			pTemp = osal_strchr(pDtr, ' ');
		} else {
			STP_DBG_PR_ERR("parser str is NULL,substring(%s)\n", parser_sub_string[type]);
			return -3;
		}

		if (pTemp == NULL) {
			STP_DBG_PR_ERR("delimiter( ) is not found,substring(%s)\n",
					 parser_sub_string[type]);
			return -4;
		}

		len = pTemp - pDtr;
		len = (len >= STP_ASSERT_TYPE_SIZE) ? STP_ASSERT_TYPE_SIZE - 1 : len;
		osal_memcpy(&tempBuf[0], pDtr, len);
		tempBuf[len] = '\0';
		ret = osal_strtol(tempBuf, 16, &res);
		if (ret) {
			STP_DBG_PR_ERR("get fw task id fail(%d)\n", ret);
			return -4;
		}
		g_stp_dbg_cpupcr->fwTaskId = (UINT32)res;

		STP_DBG_PR_INFO("fw task id :%x\n", (UINT32)res);
		break;
	case STP_DBG_FW_ISR:
		pDtr = osal_strstr(pStr, parser_sub_string[type]);

		if (pDtr != NULL) {
			pDtr += osal_strlen(parser_sub_string[type]);
			pTemp = osal_strchr(pDtr, ',');
		} else {
			STP_DBG_PR_ERR("parser str is NULL,substring(%s)\n",
					parser_sub_string[type]);
			return -3;
		}

		if (pTemp == NULL) {
			STP_DBG_PR_ERR("delimiter(,) is not found,substring(%s)\n",
					 parser_sub_string[type]);
			return -4;
		}

		len = pTemp - pDtr;
		len = (len >= STP_ASSERT_TYPE_SIZE) ? STP_ASSERT_TYPE_SIZE - 1 : len;
		osal_memcpy(&tempBuf[0], pDtr, len);
		tempBuf[len] = '\0';
		ret = osal_strtol(tempBuf, 16, &res);
		if (ret) {
			STP_DBG_PR_ERR("get fw isr id fail(%d)\n", ret);
			return -4;
		}
		g_stp_dbg_cpupcr->fwIsr = (UINT32)res;

		STP_DBG_PR_INFO("fw isr str:%x\n", (UINT32)res);
		break;
	case STP_DBG_FW_IRQ:
		pDtr = osal_strstr(pStr, parser_sub_string[type]);
		if (pDtr != NULL) {
			pDtr += osal_strlen(parser_sub_string[type]);
			pTemp = osal_strchr(pDtr, ',');
		} else {
			STP_DBG_PR_ERR("parser str is NULL,substring(%s)\n", parser_sub_string[type]);
			return -3;
		}

		if (pTemp == NULL) {
			STP_DBG_PR_ERR("delimiter(,) is not found,substring(%s)\n",
					 parser_sub_string[type]);
			return -4;
		}

		len = pTemp - pDtr;
		len = (len >= STP_ASSERT_TYPE_SIZE) ? STP_ASSERT_TYPE_SIZE - 1 : len;
		osal_memcpy(&tempBuf[0], pDtr, len);
		tempBuf[len] = '\0';
		ret = osal_strtol(tempBuf, 16, &res);
		if (ret) {
			STP_DBG_PR_ERR("get fw irq id fail(%d)\n", ret);
			return -4;
		}
		g_stp_dbg_cpupcr->fwRrq = (UINT32)res;

		STP_DBG_PR_INFO("fw irq value:%x\n", (UINT32)res);
		break;
	case STP_DBG_ASSERT_TYPE:
		pDtr = osal_strstr(pStr, parser_sub_string[type]);
		if (pDtr != NULL) {
			pDtr += osal_strlen(parser_sub_string[type]);
			pTemp = osal_strchr(pDtr, ',');
		} else {
			STP_DBG_PR_ERR("parser str is NULL,substring(%s)\n", parser_sub_string[type]);
			return -3;
		}

		if (pTemp == NULL) {
			STP_DBG_PR_ERR("delimiter(,) is not found,substring(%s)\n",
					 parser_sub_string[type]);
			return -4;
		}

		len = pTemp - pDtr;
		len = (len >= STP_ASSERT_TYPE_SIZE) ? STP_ASSERT_TYPE_SIZE - 1 : len;
		osal_memcpy(&tempBuf[0], pDtr, len);
		tempBuf[len] = '\0';

		if (osal_memcmp(tempBuf, "*", osal_strlen("*")) == 0)
			osal_memcpy(&g_stp_dbg_cpupcr->assert_type[0], "general assert",
					osal_strlen("general assert"));
		if (osal_memcmp(tempBuf, WDT_INFO_HEAD, osal_strlen(WDT_INFO_HEAD)) == 0)
			osal_memcpy(&g_stp_dbg_cpupcr->assert_type[0], "wdt", osal_strlen("wdt"));
		if (osal_memcmp(tempBuf, "RB_FULL", osal_strlen("RB_FULL")) == 0) {
			osal_memcpy(&g_stp_dbg_cpupcr->assert_type[0], tempBuf, len);

			pDtr = osal_strstr(&g_stp_dbg_cpupcr->assert_type[0], "RB_FULL(");
			if (pDtr != NULL) {
				pDtr += osal_strlen("RB_FULL(");
				pTemp = osal_strchr(pDtr, ')');
			} else {
				STP_DBG_PR_ERR("parser str is NULL,substring(RB_FULL()\n");
				return -5;
			}
			len = pTemp - pDtr;
			len = (len >= STP_ASSERT_TYPE_SIZE) ? STP_ASSERT_TYPE_SIZE - 1 : len;
			osal_memcpy(&tempBuf[0], pDtr, len);
			tempBuf[len] = '\0';
			ret = osal_strtol(tempBuf, 16, &res);
			if (ret) {
				STP_DBG_PR_ERR("get fw task id fail(%d)\n", ret);
				return -5;
			}
			g_stp_dbg_cpupcr->fwTaskId = (UINT32)res;

			STP_DBG_PR_INFO("update fw task id :%x\n", (UINT32)res);
		}

		STP_DBG_PR_INFO("fw asert type:%s\n", g_stp_dbg_cpupcr->assert_type);
		break;
	default:
		STP_DBG_PR_ERR("unknown parser type\n");
		break;
	}

	return 0;
}

static _osal_inline_ P_STP_DBG_CPUPCR_T stp_dbg_cpupcr_init(VOID)
{
	P_STP_DBG_CPUPCR_T pSdCpupcr = NULL;

	pSdCpupcr = (P_STP_DBG_CPUPCR_T) osal_malloc(osal_sizeof(STP_DBG_CPUPCR_T));
	if (!pSdCpupcr) {
		STP_DBG_PR_ERR("stp dbg cpupcr allocate memory fail!\n");
		return NULL;
	}

	osal_memset(pSdCpupcr, 0, osal_sizeof(STP_DBG_CPUPCR_T));

	osal_sleepable_lock_init(&pSdCpupcr->lock);

	return pSdCpupcr;
}

static _osal_inline_ VOID stp_dbg_cpupcr_deinit(P_STP_DBG_CPUPCR_T pCpupcr)
{
	if (pCpupcr) {
		osal_sleepable_lock_deinit(&pCpupcr->lock);
		osal_free(pCpupcr);
		pCpupcr = NULL;
	}
}

static _osal_inline_ P_STP_DBG_DMAREGS_T stp_dbg_dmaregs_init(VOID)
{
	P_STP_DBG_DMAREGS_T pDmaRegs = NULL;

	pDmaRegs = (P_STP_DBG_DMAREGS_T) osal_malloc(osal_sizeof(STP_DBG_DMAREGS_T));
	if (!pDmaRegs) {
		STP_DBG_PR_ERR("stp dbg dmareg allocate memory fail!\n");
		return NULL;
	}

	osal_memset(pDmaRegs, 0, osal_sizeof(STP_DBG_DMAREGS_T));

	osal_sleepable_lock_init(&pDmaRegs->lock);

	return pDmaRegs;
}

static VOID stp_dbg_dmaregs_deinit(P_STP_DBG_DMAREGS_T pDmaRegs)
{
	if (pDmaRegs) {
		osal_sleepable_lock_deinit(&pDmaRegs->lock);
		osal_free(pDmaRegs);
		pDmaRegs = NULL;
	}
}

/*
 *	who call this ?
 *	- stp_dbg_soc_paged_dump
 *		generate coredump and coredump timeout
 *	- wmt_dbg_poll_cpupcr
 *		dump cpupcr through command
 *	- mtk_stp_dbg_poll_cpupcr (should remove this)
 *		export to other drivers
 *	- _stp_btm_handler
 *		coredump timeout
 *	- wmt_ctrl_rx
 *		rx timeout
 *	- stp_do_tx_timeout
 *		tx timeout
 *
 */
INT32 stp_dbg_poll_cpupcr(UINT32 times, UINT32 sleep, UINT32 cmd)
{
	INT32 i = 0;
	UINT32 value = 0x0;
	ENUM_WMT_CHIP_TYPE chip_type;
	UINT8 cccr_value = 0x0;
	INT32 chip_id = -1;
	INT32 i_ret = 0;
	INT32 count = 0;

	if (!g_stp_dbg_cpupcr) {
		STP_DBG_PR_ERR("NULL reference pointer\n");
		return -1;
	}

	chip_type = wmt_detect_get_chip_type();

	if (times > STP_DBG_CPUPCR_NUM)
		times = STP_DBG_CPUPCR_NUM;

	switch (chip_type) {
	case WMT_CHIP_TYPE_COMBO:
		osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
		for (i = 0; i < times; i++) {
			stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT,
					g_stp_sdio_host_info.sdio_cltctx, SWPCDBGR, &value, 0);
			g_stp_dbg_cpupcr->buffer[g_stp_dbg_cpupcr->count] = value;
			osal_get_local_time(&(g_stp_dbg_cpupcr->sec_buffer[g_stp_dbg_cpupcr->count]),
					&(g_stp_dbg_cpupcr->nsec_buffer[g_stp_dbg_cpupcr->count]));
			if (sleep > 0)
				osal_sleep_ms(sleep);
			g_stp_dbg_cpupcr->count++;
			if (g_stp_dbg_cpupcr->count >= STP_DBG_CPUPCR_NUM)
				g_stp_dbg_cpupcr->count = 0;
		}
		osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
		break;
	case WMT_CHIP_TYPE_SOC:
		if (times > WMT_CORE_DMP_CPUPCR_NUM)
			times = WMT_CORE_DMP_CPUPCR_NUM;
		if (wmt_lib_dmp_consys_state(&g_dmp_info, times, sleep) == MTK_WCN_BOOL_TRUE) {
			osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
			for (i = 0; i < times; i++) {
				g_stp_dbg_cpupcr->buffer[g_stp_dbg_cpupcr->count] = g_dmp_info.cpu_pcr[i];
				osal_get_local_time(&(g_stp_dbg_cpupcr->sec_buffer[g_stp_dbg_cpupcr->count]),
					&(g_stp_dbg_cpupcr->nsec_buffer[g_stp_dbg_cpupcr->count]));
				g_stp_dbg_cpupcr->count++;
				if (g_stp_dbg_cpupcr->count >= STP_DBG_CPUPCR_NUM)
					g_stp_dbg_cpupcr->count = 0;
			}
			osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
		}
		break;
	default:
		STP_DBG_PR_INFO("error chip type(%d)\n", chip_type);
	}

	if (cmd) {
		UINT8 str[DBG_LOG_STR_SIZE] = {""};
		PUINT8 p = str;
		INT32 str_len = 0;

		for (i = 0; i < STP_DBG_CPUPCR_NUM; i++) {
			if (g_stp_dbg_cpupcr->sec_buffer[i] == 0 &&
			    g_stp_dbg_cpupcr->nsec_buffer[i] == 0)
				continue;

			count++;
			if (count % 4 != 0) {
				str_len = osal_sprintf(p, "%llu.%06lu/0x%08x;",
						       g_stp_dbg_cpupcr->sec_buffer[i],
						       g_stp_dbg_cpupcr->nsec_buffer[i],
						       g_stp_dbg_cpupcr->buffer[i]);
				p += str_len;
			} else {
				str_len = osal_sprintf(p, "%llu.%06lu/0x%08x;",
						       g_stp_dbg_cpupcr->sec_buffer[i],
						       g_stp_dbg_cpupcr->nsec_buffer[i],
						       g_stp_dbg_cpupcr->buffer[i]);
				STP_DBG_PR_INFO("TIME/CPUPCR: %s\n", str);
				p = str;
			}
		}
		if (count % 4 != 0)
			STP_DBG_PR_INFO("TIME/CPUPCR: %s\n", str);

		if (wmt_lib_power_lock_trylock()) {
			if (chip_type == WMT_CHIP_TYPE_SOC && wmt_lib_reg_readable()) {
				STP_DBG_PR_INFO("CONNSYS cpu:0x%x/bus:0x%x/dbg_cr1:0x%x/dbg_cr2:0x%x/EMIaddr:0x%x\n",
					  stp_dbg_soc_read_debug_crs(CONNSYS_CPU_CLK),
					  stp_dbg_soc_read_debug_crs(CONNSYS_BUS_CLK),
					  stp_dbg_soc_read_debug_crs(CONNSYS_DEBUG_CR1),
					  stp_dbg_soc_read_debug_crs(CONNSYS_DEBUG_CR2),
					  stp_dbg_soc_read_debug_crs(CONNSYS_EMI_REMAP));
			}
			wmt_lib_power_lock_release();
		}

		chip_id = mtk_wcn_wmt_chipid_query();
		if (chip_id == 0x6632) {
			for (i = 0; i < 8; i++) {
				i_ret = mtk_wcn_hif_sdio_f0_readb(g_stp_sdio_host_info.sdio_cltctx,
						CCCR_F8 + i, &cccr_value);
				if (i_ret)
					STP_DBG_PR_ERR("read CCCR fail(%d), address(0x%x)\n",
							i_ret, CCCR_F8 + i);
				else
					STP_DBG_PR_INFO("read CCCR value(0x%x), address(0x%x)\n",
							cccr_value, CCCR_F8 + i);
				cccr_value = 0x0;
			}
		}
		/* Need in platform code - mtxxxx.c to provide function implementation */
		mtk_wcn_consys_hang_debug();
	}
	if (chip_type == WMT_CHIP_TYPE_COMBO) {
		STP_DBG_PR_INFO("dump sdio register for debug\n");
		mtk_stp_dump_sdio_register();
	}
	return 0;
}

INT32 stp_dbg_dump_cpupcr_reg_info(PUINT8 buf, UINT32 consys_lp_reg)
{
	INT32 i = 0;
	INT32 count = 0;
	UINT32 len = 0;

	/* never retrun negative value */
	if (!g_stp_dbg_cpupcr || !buf) {
		STP_DBG_PR_DBG("NULL pointer, g_stp_dbg_cpupcr:%p, buf:%p\n",
				g_stp_dbg_cpupcr, buf);
		return 0;
	}

	for (i = 0; i < STP_DBG_CPUPCR_NUM; i++) {
		if (g_stp_dbg_cpupcr->sec_buffer[i] == 0 &&
				g_stp_dbg_cpupcr->nsec_buffer[i] == 0)
			continue;
		count++;
		if (count == 1)
			len += osal_sprintf(buf + len, "0x%08x", g_stp_dbg_cpupcr->buffer[i]);
		else
			len += osal_sprintf(buf + len, ";0x%08x", g_stp_dbg_cpupcr->buffer[i]);
	}

	if (count == 0)
		len += osal_sprintf(buf + len, "0x%08x\n", consys_lp_reg);
	else
		len += osal_sprintf(buf + len, ";0x%08x\n", consys_lp_reg);

	stp_dbg_clear_cpupcr_reg_info();

	return len;
}

VOID stp_dbg_clear_cpupcr_reg_info(VOID)
{
	if (osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock)) {
		STP_DBG_PR_DBG("lock failed\n");
		return;
	}

	osal_memset(&g_stp_dbg_cpupcr->buffer[0], 0, STP_DBG_CPUPCR_NUM);
	g_stp_dbg_cpupcr->count = 0;
	g_stp_dbg_cpupcr->host_assert_info.reason = 0;
	g_stp_dbg_cpupcr->host_assert_info.drv_type = 0;
	g_stp_dbg_cpupcr->issue_type = STP_FW_ISSUE_TYPE_INVALID;
	g_stp_dbg_cpupcr->keyword[0] = '\0';
	osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
}

INT32 stp_dbg_poll_dmaregs(UINT32 times, UINT32 sleep)
{
#if 0
	INT32 i = 0;

	if (!g_stp_dbg_dmaregs) {
		STP_DBG_PR_ERR("NULL reference pointer\n");
		return -1;
	}

	osal_lock_sleepable_lock(&g_stp_dbg_dmaregs->lock);

	if (g_stp_dbg_dmaregs->count + times > STP_DBG_DMAREGS_NUM) {
		if (g_stp_dbg_dmaregs->count > STP_DBG_DMAREGS_NUM) {
			STP_DBG_PR_ERR("g_stp_dbg_dmaregs->count:%d must less than STP_DBG_DMAREGS_NUM:%d\n",
				g_stp_dbg_dmaregs->count, STP_DBG_DMAREGS_NUM);
			g_stp_dbg_dmaregs->count = 0;
			STP_DBG_PR_ERR("g_stp_dbg_dmaregs->count be set default value 0\n");
		}
		times = STP_DBG_DMAREGS_NUM - g_stp_dbg_dmaregs->count;
	}
	if (times > STP_DBG_DMAREGS_NUM) {
		STP_DBG_PR_ERR("times overflow, set default value:0\n");
		times = 0;
	}

	for (i = 0; i < times; i++) {
		INT32 k = 0;

		for (; k < DMA_REGS_MAX; k++) {
			STP_DBG_PR_INFO("times:%d,i:%d reg: %s, regs:%08x\n", times, i, dmaRegsStr[k],
					  wmt_plat_read_dmaregs(k));
			/* g_stp_dbg_dmaregs->dmaIssue[k][g_stp_dbg_dmaregs->count + i] =
			 * wmt_plat_read_dmaregs(k);
			 */
		}
		osal_sleep_ms(sleep);
	}

	g_stp_dbg_dmaregs->count += times;

	osal_unlock_sleepable_lock(&g_stp_dbg_dmaregs->lock);
#else
	return 0;
#endif
}

INT32 stp_dbg_poll_cpupcr_ctrl(UINT32 en)
{
	STP_DBG_PR_INFO("%s polling cpupcr\n", en == 0 ? "start" : "stop");

	osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
	g_stp_dbg_cpupcr->stop_flag = en;
	osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);

	return 0;
}

INT32 stp_dbg_set_version_info(UINT32 chipid, PUINT8 pRomVer, PUINT8 pPatchVer, PUINT8 pPatchBrh)
{
	if (g_stp_dbg_cpupcr) {
		osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
		g_stp_dbg_cpupcr->chipId = chipid;

		if (pRomVer)
			osal_memcpy(g_stp_dbg_cpupcr->romVer, pRomVer, 2);
		if (pPatchVer)
			osal_memcpy(g_stp_dbg_cpupcr->patchVer, pPatchVer, 8);
		if (pPatchBrh)
			osal_memcpy(g_stp_dbg_cpupcr->branchVer, pPatchBrh, 4);

		osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
	} else {
		STP_DBG_PR_ERR("NULL pointer\n");
		return -1;
	}

	STP_DBG_PR_DBG("chipid(0x%x),romver(%s),patchver(%s),branchver(%s)\n",
			g_stp_dbg_cpupcr->chipId,
			&g_stp_dbg_cpupcr->romVer[0],
			&g_stp_dbg_cpupcr->patchVer[0],
			&g_stp_dbg_cpupcr->branchVer[0]);

	return 0;
}

INT32 stp_dbg_set_wifiver(UINT32 wifiver)
{
	if (!g_stp_dbg_cpupcr) {
		STP_DBG_PR_ERR("NULL pointer\n");
		return -1;
	}

	osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
	g_stp_dbg_cpupcr->wifiVer = wifiver;
	osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);

	STP_DBG_PR_INFO("wifiver(%x)\n", g_stp_dbg_cpupcr->wifiVer);

	return 0;
}

INT32 stp_dbg_set_host_assert_info(UINT32 drv_type, UINT32 reason, UINT32 en)
{
	osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);

	g_stp_dbg_cpupcr->host_assert_info.assert_from_host = en;
	g_stp_dbg_cpupcr->host_assert_info.drv_type = drv_type;
	g_stp_dbg_cpupcr->host_assert_info.reason = reason;

	osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);

	return 0;
}

VOID stp_dbg_set_keyword(PINT8 keyword)
{
	osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
	if (keyword != NULL) {
		if (osal_strlen(keyword) >= STP_DBG_KEYWORD_SIZE)
			STP_DBG_PR_INFO("Keyword over max size(%d)\n", STP_DBG_KEYWORD_SIZE);
		else if (osal_strchr(keyword, '<') != NULL || osal_strchr(keyword, '>') != NULL)
			STP_DBG_PR_INFO("Keyword has < or >, keywrod: %s\n", keyword);
		else
			osal_strncat(&g_stp_dbg_cpupcr->keyword[0], keyword, osal_strlen(keyword));
	} else {
		g_stp_dbg_cpupcr->keyword[0] = '\0';
	}
	osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
}

UINT32 stp_dbg_get_host_trigger_assert(VOID)
{
	return g_stp_dbg_cpupcr->host_assert_info.assert_from_host;
}

VOID stp_dbg_set_coredump_timer_state(CORE_DUMP_STA state)
{
	if (g_core_dump)
		g_core_dump->sm = state;
}

INT32 stp_dbg_get_coredump_timer_state(VOID)
{
	if (g_core_dump)
		return g_core_dump->sm;
	return -1;
}

INT32 stp_dbg_set_fw_info(PUINT8 issue_info, UINT32 len, ENUM_STP_FW_ISSUE_TYPE issue_type)
{
	ENUM_ASSERT_INFO_PARSER_TYPE type_index;
	PUINT8 tempbuf = NULL;
	UINT32 i = 0;
	INT32 iRet = 0;

	if (issue_info == NULL) {
		STP_DBG_PR_ERR("null issue infor\n");
		return -1;
	}

	if (g_stp_dbg_cpupcr->issue_type &&
	    g_stp_dbg_cpupcr->issue_type != STP_HOST_TRIGGER_COLLECT_FTRACE) {
		STP_DBG_PR_ERR("assert information has been set up\n");
		return -1;
	}

	STP_DBG_PR_INFO("issue type(%d)\n", issue_type);
	g_stp_dbg_cpupcr->issue_type = issue_type;
	osal_memset(&g_stp_dbg_cpupcr->assert_info[0], 0, STP_ASSERT_INFO_SIZE);

	/*print patch version when assert happened */
	STP_DBG_PR_INFO("[consys patch]patch version:%s\n", g_stp_dbg_cpupcr->patchVer);
	STP_DBG_PR_INFO("[consys patch]ALPS branch:%s\n", g_stp_dbg_cpupcr->branchVer);

	if ((issue_type == STP_FW_ASSERT_ISSUE) ||
	    (issue_type == STP_HOST_TRIGGER_FW_ASSERT) ||
	    (issue_type == STP_HOST_TRIGGER_ASSERT_TIMEOUT) ||
	    (issue_type == STP_HOST_TRIGGER_COLLECT_FTRACE) ||
	    (issue_type == STP_FW_ABT)) {
		if ((issue_type == STP_FW_ASSERT_ISSUE) || (issue_type == STP_HOST_TRIGGER_FW_ASSERT)
			|| (issue_type == STP_FW_ABT)) {
			tempbuf = osal_malloc(len + 1);
			if (!tempbuf)
				return -2;

			osal_memcpy(&tempbuf[0], issue_info, len);

			for (i = 0; i < len; i++) {
				if (tempbuf[i] == '\0')
					tempbuf[i] = '?';
			}

			tempbuf[len] = '\0';

			for (type_index = STP_DBG_ASSERT_INFO; type_index < STP_DBG_PARSER_TYPE_MAX;
					type_index++) {
				iRet = stp_dbg_parser_assert_str(&tempbuf[0], type_index);
				if (iRet)
					STP_DBG_PR_INFO("fail to parse assert str %s, type = %d, ret = %d\n",
						&tempbuf[0], type_index, iRet);
			}

		}
		if ((issue_type == STP_HOST_TRIGGER_FW_ASSERT) ||
		    (issue_type == STP_HOST_TRIGGER_ASSERT_TIMEOUT) ||
		    (issue_type == STP_HOST_TRIGGER_COLLECT_FTRACE)) {
			g_stp_dbg_cpupcr->fwIsr = 0;
			g_stp_dbg_cpupcr->fwRrq = 0;

			osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
			switch (g_stp_dbg_cpupcr->host_assert_info.drv_type) {
			case WMTDRV_TYPE_BT:
				STP_DBG_PR_INFO("BT trigger assert\n");
				if (g_stp_dbg_cpupcr->host_assert_info.reason != 31)
					g_stp_dbg_cpupcr->fwTaskId = STP_DBG_TASK_BT; /*BT firmware trigger assert */
				else {
					/*BT stack trigger assert */
					g_stp_dbg_cpupcr->fwTaskId = STP_DBG_TASK_NATBT;
				}
				break;
			case WMTDRV_TYPE_FM:
				STP_DBG_PR_INFO("FM trigger assert\n");
				g_stp_dbg_cpupcr->fwTaskId = STP_DBG_TASK_FM;
				break;
			case WMTDRV_TYPE_GPS:
				STP_DBG_PR_INFO("GPS trigger assert\n");
				g_stp_dbg_cpupcr->fwTaskId = STP_DBG_TASK_DRVGPS;
				break;
			case WMTDRV_TYPE_GPSL5:
				STP_DBG_PR_INFO("GPSL5 trigger assert\n");
				g_stp_dbg_cpupcr->fwTaskId = STP_DBG_TASK_DRVGPS;
				break;
			case WMTDRV_TYPE_WIFI:
				STP_DBG_PR_INFO("WIFI trigger assert\n");
				g_stp_dbg_cpupcr->fwTaskId = STP_DBG_TASK_DRVWIFI;
				break;
			case WMTDRV_TYPE_WMT:
				STP_DBG_PR_INFO("WMT trigger assert\n");
				if (issue_type == STP_HOST_TRIGGER_ASSERT_TIMEOUT)
					osal_memcpy(&g_stp_dbg_cpupcr->assert_info[0], issue_info, len);
				/* 30: adb trigger assert */
				/* 43: process packet fail count > 10 */
				/* 44: rx timeout with pending data */
				/* 45: tx timeout with pending data */
				if (g_stp_dbg_cpupcr->host_assert_info.reason == 30 ||
				    g_stp_dbg_cpupcr->host_assert_info.reason == 43 ||
				    g_stp_dbg_cpupcr->host_assert_info.reason == 44 ||
				    g_stp_dbg_cpupcr->host_assert_info.reason == 45)
					g_stp_dbg_cpupcr->fwTaskId = STP_DBG_TASK_DRVSTP;
				else
					g_stp_dbg_cpupcr->fwTaskId = STP_DBG_TASK_WMT;
				break;
			default:
				break;
			}
			g_stp_dbg_cpupcr->host_assert_info.assert_from_host = 0;
			osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);

		} else if (issue_type == STP_FW_ABT) {
			INT32 copyLen = (len < STP_ASSERT_INFO_SIZE ? len : STP_ASSERT_INFO_SIZE - 1);

			osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
			osal_memcpy(&g_stp_dbg_cpupcr->assert_info[0], tempbuf, copyLen);
			g_stp_dbg_cpupcr->assert_info[copyLen] = '\0';
			osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
		}

		if (tempbuf)
			osal_free(tempbuf);
	} else if (issue_type == STP_FW_NOACK_ISSUE) {
		osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
		osal_memcpy(&g_stp_dbg_cpupcr->assert_info[0], issue_info, len);
		g_stp_dbg_cpupcr->fwTaskId = STP_DBG_TASK_DRVSTP;
		g_stp_dbg_cpupcr->fwRrq = 0;
		g_stp_dbg_cpupcr->fwIsr = 0;
		osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
	} else if (issue_type == STP_DBG_PROC_TEST) {
		osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
		osal_memcpy(&g_stp_dbg_cpupcr->assert_info[0], issue_info, len);
		g_stp_dbg_cpupcr->fwTaskId = STP_DBG_TASK_WMT;
		g_stp_dbg_cpupcr->fwRrq = 0;
		g_stp_dbg_cpupcr->fwIsr = 0;
		osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
	} else if (issue_type == STP_FW_WARM_RST_ISSUE) {
		osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
		osal_memcpy(&g_stp_dbg_cpupcr->assert_info[0], issue_info, len);
		g_stp_dbg_cpupcr->fwTaskId = STP_DBG_TASK_WMT;
		g_stp_dbg_cpupcr->fwRrq = 0;
		g_stp_dbg_cpupcr->fwIsr = 0;
		osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
	} else {
		STP_DBG_PR_ERR("invalid issue type(%d)\n", issue_type);
		return -3;
	}

	return 0;
}

INT32 stp_dbg_cpupcr_infor_format(PUINT8 buf, UINT32 max_len)
{
	UINT32 len = 0;
	UINT32 i = 0;

	/* never retrun negative value */
	if (!g_stp_dbg_cpupcr || !buf) {
		STP_DBG_PR_ERR("NULL pointer, g_stp_dbg_cpupcr:%p, buf:%p\n",
				 g_stp_dbg_cpupcr, buf);
		return 0;
	}

	/* format common information about issue */
	/* max_len can guarantee there's enough buffer for <main> section */
	len = osal_sprintf(buf, "<main>\n\t");
	len += osal_sprintf(buf + len, "<chipid>\n\t\tMT%x\n\t</chipid>\n\t",
			g_stp_dbg_cpupcr->chipId);
	len += osal_sprintf(buf + len, "<version>\n\t\t");
	len += osal_sprintf(buf + len, "<rom>%s</rom>\n\t\t", g_stp_dbg_cpupcr->romVer);
	if (!(osal_memcmp(g_stp_dbg_cpupcr->branchVer, "ALPS", strlen("ALPS"))))
		len += osal_sprintf(buf + len, "<branch>Internal Dev</branch>\n\t\t",
				g_stp_dbg_cpupcr->branchVer);
	else
		len += osal_sprintf(buf + len, "<branch>W%sMP</branch>\n\t\t",
				g_stp_dbg_cpupcr->branchVer);

	len += osal_sprintf(buf + len, "<patch>%s</patch>\n\t\t", g_stp_dbg_cpupcr->patchVer);

	if (g_stp_dbg_cpupcr->wifiVer == 0)
		len += osal_sprintf(buf + len, "<wifi>NULL</wifi>\n\t");
	else
		len += osal_sprintf(buf + len, "<wifi>0x%X.%X</wifi>\n\t",
				(UINT8)((g_stp_dbg_cpupcr->wifiVer & 0xFF00)>>8),
				(UINT8)(g_stp_dbg_cpupcr->wifiVer & 0xFF));

	len += osal_sprintf(buf + len, "</version>\n\t");

	/*format issue information: no ack, assert */
	len += osal_sprintf(buf + len, "<issue>\n\t\t<classification>\n\t\t\t");
	if ((g_stp_dbg_cpupcr->issue_type == STP_FW_NOACK_ISSUE) ||
	    (g_stp_dbg_cpupcr->issue_type == STP_DBG_PROC_TEST) ||
	    (g_stp_dbg_cpupcr->issue_type == STP_FW_WARM_RST_ISSUE) ||
	    (g_stp_dbg_cpupcr->issue_type == STP_FW_ABT)) {
		len += osal_sprintf(buf + len, "%s\n\t\t</classification>\n\t\t<rc>\n\t\t\t",
				g_stp_dbg_cpupcr->assert_info);
		len += osal_sprintf(buf + len, "NULL\n\t\t</rc>\n\t</issue>\n\t");
		len += osal_sprintf(buf + len, "<hint>\n\t\t<time_align>NULL</time_align>\n\t\t");
		len += osal_sprintf(buf + len, "<host>NULL</host>\n\t\t");
		len += osal_sprintf(buf + len, "<client>\n\t\t\t<task>%s</task>\n\t\t\t",
				stp_dbg_id_to_task(g_stp_dbg_cpupcr->fwTaskId));
		len += osal_sprintf(buf + len, "<irqx>IRQ_0x%x</irqx>\n\t\t\t", g_stp_dbg_cpupcr->fwRrq);
		len += osal_sprintf(buf + len, "<isr>0x%x</isr>\n\t\t\t", g_stp_dbg_cpupcr->fwIsr);
		len += osal_sprintf(buf + len, "<drv_type>NULL</drv_type>\n\t\t\t");
		len += osal_sprintf(buf + len, "<reason>NULL</reason>\n\t\t\t");
		len += osal_sprintf(buf + len, "<keyword>%s</keyword>\n\t\t\t",
			g_stp_dbg_cpupcr->keyword);
	} else if ((g_stp_dbg_cpupcr->issue_type == STP_FW_ASSERT_ISSUE) ||
		   (g_stp_dbg_cpupcr->issue_type == STP_HOST_TRIGGER_FW_ASSERT) ||
		   (g_stp_dbg_cpupcr->issue_type == STP_HOST_TRIGGER_ASSERT_TIMEOUT)) {
		len += osal_sprintf(buf + len, "%s\n\t\t</classification>\n\t\t<rc>\n\t\t\t",
				g_stp_dbg_cpupcr->assert_info);
		len += osal_sprintf(buf + len, "%s\n\t\t</rc>\n\t</issue>\n\t",
				g_stp_dbg_cpupcr->assert_type);
		len += osal_sprintf(buf + len, "<hint>\n\t\t<time_align>NULL</time_align>\n\t\t");
		len += osal_sprintf(buf + len, "<host>NULL</host>\n\t\t");
		len += osal_sprintf(buf + len, "<client>\n\t\t\t<task>%s</task>\n\t\t\t",
				stp_dbg_id_to_task(g_stp_dbg_cpupcr->fwTaskId));
		if (g_stp_dbg_cpupcr->host_assert_info.reason == 32 ||
				g_stp_dbg_cpupcr->host_assert_info.reason == 33 ||
				g_stp_dbg_cpupcr->host_assert_info.reason == 34 ||
				g_stp_dbg_cpupcr->host_assert_info.reason == 35 ||
				g_stp_dbg_cpupcr->host_assert_info.reason == 36 ||
				g_stp_dbg_cpupcr->host_assert_info.reason == 37 ||
				g_stp_dbg_cpupcr->host_assert_info.reason == 38 ||
				g_stp_dbg_cpupcr->host_assert_info.reason == 39 ||
				g_stp_dbg_cpupcr->host_assert_info.reason == 40) {
			/*handling wmt turn on/off bt cmd has ack but no evt issue */
			/*one of both the irqx and irs is nULL, then use task to find MOF */
			len += osal_sprintf(buf + len, "<irqx>NULL</irqx>\n\t\t\t");
		} else
			len += osal_sprintf(buf + len, "<irqx>IRQ_0x%x</irqx>\n\t\t\t",
					g_stp_dbg_cpupcr->fwRrq);
		len += osal_sprintf(buf + len, "<isr>0x%x</isr>\n\t\t\t", g_stp_dbg_cpupcr->fwIsr);

		if (g_stp_dbg_cpupcr->issue_type == STP_FW_ASSERT_ISSUE) {
			len += osal_sprintf(buf + len, "<drv_type>NULL</drv_type>\n\t\t\t");
			len += osal_sprintf(buf + len, "<reason>NULL</reason>\n\t\t\t");
		}

		if ((g_stp_dbg_cpupcr->issue_type == STP_HOST_TRIGGER_FW_ASSERT) ||
		    (g_stp_dbg_cpupcr->issue_type == STP_HOST_TRIGGER_ASSERT_TIMEOUT)) {
			len += osal_sprintf(buf + len, "<drv_type>%d</drv_type>\n\t\t\t",
					g_stp_dbg_cpupcr->host_assert_info.drv_type);
			len += osal_sprintf(buf + len, "<reason>%d</reason>\n\t\t\t",
					g_stp_dbg_cpupcr->host_assert_info.reason);
		}

		len += osal_sprintf(buf + len, "<keyword>%s</keyword>\n\t\t\t",
			g_stp_dbg_cpupcr->keyword);
	} else {
		len += osal_sprintf(buf + len, "NULL\n\t\t</classification>\n\t\t<rc>\n\t\t\t");
		len += osal_sprintf(buf + len, "NULL\n\t\t</rc>\n\t</issue>\n\t");
		len += osal_sprintf(buf + len, "<hint>\n\t\t<time_align>NULL</time_align>\n\t\t");
		len += osal_sprintf(buf + len, "<host>NULL</host>\n\t\t");
		len += osal_sprintf(buf + len, "<client>\n\t\t\t<task>%s</task>\n\t\t\t",
				    stp_dbg_id_to_task(g_stp_dbg_cpupcr->fwTaskId));
		len += osal_sprintf(buf + len, "<irqx>NULL</irqx>\n\t\t\t");
		len += osal_sprintf(buf + len, "<isr>NULL</isr>\n\t\t\t");
		len += osal_sprintf(buf + len, "<drv_type>NULL</drv_type>\n\t\t\t");
		len += osal_sprintf(buf + len, "<reason>NULL</reason>\n\t\t\t");
		len += osal_sprintf(buf + len, "<keyword>%s</keyword>\n\t\t\t",
			g_stp_dbg_cpupcr->keyword);
	}

	len += osal_sprintf(buf + len, "<pctrace>");
	STP_DBG_PR_INFO("stp-dbg:sub len1 for debug(%d)\n", len);

	if (!g_stp_dbg_cpupcr->count)
		len += osal_sprintf(buf + len, "NULL");
	else {
		for (i = 0; i < g_stp_dbg_cpupcr->count; i++)
			len += osal_sprintf(buf + len, "%08x,", g_stp_dbg_cpupcr->buffer[i]);
	}
	STP_DBG_PR_INFO("stp-dbg:sub len2 for debug(%d)\n", len);
	len += osal_sprintf(buf + len, "</pctrace>\n\t\t\t");
	len += osal_sprintf(buf + len,
			"<extension>NULL</extension>\n\t\t</client>\n\t</hint>\n</main>\n");

	STP_DBG_PR_INFO("buffer len[%d]\n", len);
	/* STP_DBG_PR_INFO("Format infor:\n%s\n",buf); */

	osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
	osal_memset(&g_stp_dbg_cpupcr->buffer[0], 0, STP_DBG_CPUPCR_NUM);
	g_stp_dbg_cpupcr->count = 0;
	g_stp_dbg_cpupcr->host_assert_info.reason = 0;
	g_stp_dbg_cpupcr->host_assert_info.drv_type = 0;
	g_stp_dbg_cpupcr->issue_type = STP_FW_ISSUE_TYPE_INVALID;
	g_stp_dbg_cpupcr->keyword[0] = '\0';
	g_stp_dbg_cpupcr->fwRrq = 0;
	g_stp_dbg_cpupcr->fwIsr = 0;
	osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);

	return len;
}

PUINT8 stp_dbg_id_to_task(UINT32 id)
{
	ENUM_WMT_CHIP_TYPE chip_type;
	PUINT8 task_id = NULL;

	chip_type = wmt_detect_get_chip_type();
	switch (chip_type) {
	case WMT_CHIP_TYPE_COMBO:
		task_id = stp_dbg_combo_id_to_task(id);
		break;
	case WMT_CHIP_TYPE_SOC:
		task_id = stp_dbg_soc_id_to_task(id);
		break;
	default:
		STP_DBG_PR_ERR("error chip type(%d)\n", chip_type);
	}

	return task_id;
}

VOID stp_dbg_reset(VOID)
{
	if (g_stp_dbg_cpupcr) {
		osal_memset(g_stp_dbg_cpupcr->buffer, 0, osal_sizeof(g_stp_dbg_cpupcr->buffer));
		osal_memset(g_stp_dbg_cpupcr->sec_buffer, 0, osal_sizeof(g_stp_dbg_cpupcr->sec_buffer));
		osal_memset(g_stp_dbg_cpupcr->nsec_buffer, 0, osal_sizeof(g_stp_dbg_cpupcr->nsec_buffer));
	}

	if (g_stp_dbg_dmaregs) {
		g_stp_dbg_dmaregs->count = 0;
		osal_memset(g_stp_dbg_dmaregs->dmaIssue, 0, osal_sizeof(g_stp_dbg_dmaregs->dmaIssue));
	}
}

MTKSTP_DBG_T *stp_dbg_init(PVOID btm_half)
{
	MTKSTP_DBG_T *stp_dbg = NULL;

	stp_dbg = kzalloc(sizeof(MTKSTP_DBG_T), GFP_KERNEL);
	if (stp_dbg == NULL)
		goto ERR_EXIT1;
	if (IS_ERR(stp_dbg)) {
		STP_DBG_PR_ERR("-ENOMEM\n");
		goto ERR_EXIT1;
	}

	stp_dbg->logsys = vmalloc(sizeof(MTKSTP_LOG_SYS_T));
	if (stp_dbg->logsys == NULL)
		goto ERR_EXIT2;
	if (IS_ERR(stp_dbg->logsys)) {
		STP_DBG_PR_ERR("-ENOMEM stp_gdb->logsys\n");
		goto ERR_EXIT2;
	}
	memset(stp_dbg->logsys, 0, sizeof(MTKSTP_LOG_SYS_T));
	spin_lock_init(&(stp_dbg->logsys->lock));
	INIT_WORK(&(stp_dbg->logsys->dump_work), stp_dbg_dmp_print_work);
	stp_dbg->pkt_trace_no = 0;
	stp_dbg->is_enable = 0;
	g_stp_dbg = stp_dbg;

	if (btm_half != NULL)
		stp_dbg->btm = btm_half;
	else
		stp_dbg->btm = NULL;

	g_core_dump = stp_dbg_core_dump_init(STP_CORE_DUMP_TIMEOUT);
	if (!g_core_dump) {
		STP_DBG_PR_ERR("-ENOMEM wcn_coer_dump_init fail!");
		goto ERR_EXIT2;
	}
	g_stp_dbg_cpupcr = stp_dbg_cpupcr_init();
	if (!g_stp_dbg_cpupcr) {
		STP_DBG_PR_ERR("-ENOMEM stp_dbg_cpupcr_init fail!");
		goto ERR_EXIT2;
	}
	g_stp_dbg_dmaregs = stp_dbg_dmaregs_init();
	if (!g_stp_dbg_dmaregs) {
		STP_DBG_PR_ERR("-ENOMEM stp_dbg_dmaregs_init fail!");
		goto ERR_EXIT2;
	}
	return stp_dbg;

ERR_EXIT2:
	stp_dbg_deinit(stp_dbg);
	return NULL;

ERR_EXIT1:
	kfree(stp_dbg);
	return NULL;
}

INT32 stp_dbg_deinit(MTKSTP_DBG_T *stp_dbg)
{
	stp_dbg_core_dump_deinit(g_core_dump);

	stp_dbg_cpupcr_deinit(g_stp_dbg_cpupcr);
	stp_dbg_dmaregs_deinit(g_stp_dbg_dmaregs);
	/* unbind with netlink */
	stp_dbg_nl_deinit();

	if (stp_dbg->logsys)
		vfree(stp_dbg->logsys);

	kfree(stp_dbg);

	return 0;
}

INT32 stp_dbg_start_coredump_timer(VOID)
{
	if (!g_core_dump) {
		STP_DBG_PR_ERR("invalid pointer!\n");
		return -1;
	}

	return osal_timer_modify(&g_core_dump->dmp_timer, STP_CORE_DUMP_TIMEOUT);
}

INT32 stp_dbg_start_emi_dump(VOID)
{
	INT32 ret = 0;

	if (!g_core_dump) {
		STP_DBG_PR_ERR("invalid pointer!\n");
		return -1;
	}

	if (mtk_wcn_wlan_emi_mpu_set_protection)
		(*mtk_wcn_wlan_emi_mpu_set_protection)(false);
	/* Disable MCIF EMI protection */
	mtk_wcn_wmt_set_mcif_mpu_protection(false);
	stp_dbg_set_coredump_timer_state(CORE_DUMP_DOING);
	osal_timer_modify(&g_core_dump->dmp_emi_timer, STP_EMI_DUMP_TIMEOUT);
	ret = stp_dbg_nl_send_data(EMICOREDUMP_CMD, sizeof(EMICOREDUMP_CMD));
	if (ret)
		stp_dbg_stop_emi_dump();

	return ret ? -1 : 0;
}

INT32 stp_dbg_stop_emi_dump(VOID)
{
	if (!g_core_dump) {
		STP_DBG_PR_ERR("invalid pointer!\n");
		return -1;
	}

	if (mtk_wcn_stp_emi_dump_flag_get() == 1) {
		STP_DBG_PR_ERR("stopping emi dump!\n");
		return -2;
	}

	mtk_wcn_stp_emi_dump_flag_ctrl(1);
	/* Enable MCIF EMI protection */
	mtk_wcn_wmt_set_mcif_mpu_protection(true);
	if (mtk_wcn_wlan_emi_mpu_set_protection)
		(*mtk_wcn_wlan_emi_mpu_set_protection)(true);
	osal_timer_stop(&g_core_dump->dmp_emi_timer);
	return 0;
}

INT32 stp_dbg_nl_send_data(const PINT8 buf, INT32 len)
{
	PINT8 pdata = NULL;
	INT32 ret = 0;

	pdata = kmalloc(len+5, GFP_KERNEL);
	if (!pdata)
		return -1;
	pdata[0] = '[';
	pdata[1] = 'M';
	pdata[2] = ']';
	osal_memcpy(&pdata[3], &len, 2);
	osal_memcpy(&pdata[5], buf, len);
	ret = stp_dbg_dump_send_retry_handler(pdata, len);
	kfree(pdata);
	return ret;
}
