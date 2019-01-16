#include <linux/kernel.h>	/* GFP_KERNEL */
#include <linux/timer.h>	/* init_timer, add_time, del_timer_sync */
#include <linux/time.h>		/* gettimeofday */
#include <linux/delay.h>
#include <linux/slab.h>		/* kzalloc */
#include <linux/sched.h>	/* task's status */
#include <linux/vmalloc.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <asm/atomic.h>

#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <net/genetlink.h>

#include <linux/zlib.h>
#include <linux/uaccess.h>
#include <linux/crc32.h>

#include "osal_typedef.h"
#include "stp_dbg.h"
/* #include "stp_btm.h" */
#include "btm_core.h"

#define PFX_STP_DBG                      "[STPDbg]"
#define STP_DBG_LOG_LOUD                 4
#define STP_DBG_LOG_DBG                  3
#define STP_DBG_LOG_INFO                 2
#define STP_DBG_LOG_WARN                 1
#define STP_DBG_LOG_ERR                  0

UINT32 gStpDbgDbgLevel = STP_DBG_LOG_INFO;
UINT32 gStpDbgLogOut = 0;
UINT32 gStpDbgDumpType = STP_DBG_PKT;
#define STP_DBG_LOUD_FUNC(fmt, arg...)	\
do {	\
	pr_debug(PFX_STP_DBG "%s: "  fmt, __func__ , ##arg);	\
} while (0)

#define STP_DBG_DBG_FUNC(fmt, arg...)	\
do {	\
	pr_debug(PFX_STP_DBG "%s: "  fmt, __func__ , ##arg);	\
} while (0)

#define STP_DBG_INFO_FUNC(fmt, arg...)	\
do {	\
	pr_warn(PFX_STP_DBG fmt, ##arg);	\
} while (0)

#define STP_DBG_WARN_FUNC(fmt, arg...)	\
do {	\
	pr_err(PFX_STP_DBG "%s: "  fmt, __func__ , ##arg);	\
} while (0)

#define STP_DBG_ERR_FUNC(fmt, arg...)	\
do {	\
	pr_err(PFX_STP_DBG "%s: "   fmt, __func__ , ##arg);	\
} while (0)

#define STP_DBG_TRC_FUNC(f)	\
do {	\
	pr_debug(PFX_STP_DBG "<%s> <%d>\n", __func__, __LINE__);	\
} while (0)


MTKSTP_DBG_T *g_stp_dbg = NULL;

#define STP_DBG_FAMILY_NAME        "STP_DBG"
#define MAX_BIND_PROCESS    (4)

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

static struct genl_family stp_dbg_gnl_family = {
	.id = GENL_ID_GENERATE,
	.hdrsize = 0,
	.name = STP_DBG_FAMILY_NAME,
	.version = 1,
	.maxattr = STP_DBG_ATTR_MAX,
};

static void stp_dbg_nl_init(void);
static void stp_dbg_nl_deinit(void);
static int stp_dbg_nl_bind(struct sk_buff *skb, struct genl_info *info);
static int stp_dbg_nl_reset(struct sk_buff *skb, struct genl_info *info);
static INT32 _wcn_core_dump_post_handle(P_WCN_CORE_DUMP_T dmp);
static UINT32 stp_dbg_get_host_trigger_assert(VOID);

/* attribute policy */
static struct nla_policy stp_dbg_genl_policy[STP_DBG_ATTR_MAX + 1] = {
	[STP_DBG_ATTR_MSG] = {.type = NLA_NUL_STRING},
};

/* operation definition */
static struct genl_ops stp_dbg_gnl_ops_bind = {
	.cmd = STP_DBG_COMMAND_BIND,
	.flags = 0,
	.policy = stp_dbg_genl_policy,
	.doit = stp_dbg_nl_bind,
	.dumpit = NULL,
};

static struct genl_ops stp_dbg_gnl_ops_reset = {
	.cmd = STP_DBG_COMMAND_RESET,
	.flags = 0,
	.policy = stp_dbg_genl_policy,
	.doit = stp_dbg_nl_reset,
	.dumpit = NULL,
};

static UINT32 stp_dbg_seqnum = 0;
static INT32 num_bind_process = 0;
static pid_t bind_pid[MAX_BIND_PROCESS];

static P_WCN_CORE_DUMP_T g_core_dump = NULL;
static P_STP_DBG_CPUPCR_T g_stp_dbg_cpupcr = NULL;

/* core_dump_timeout_handler - handler of coredump timeout
 * @ data - core dump object's pointer
 *
 * No return value
 */
static VOID core_dump_timeout_handler(unsigned long data)
{
	P_WCN_CORE_DUMP_T dmp = (P_WCN_CORE_DUMP_T) data;

	STP_DBG_INFO_FUNC(" start\n");

	stp_btm_notify_coredump_timeout_wq(g_stp_dbg->btm);

	STP_DBG_INFO_FUNC(" end\n");

	if (dmp) {
		STP_DBG_WARN_FUNC
		    (" coredump timer timeout, coredump maybe not finished successfully\n");
		dmp->sm = CORE_DUMP_TIMEOUT;
	}
}


/* wcn_core_dump_init - create core dump sys
 * @ timeout - core dump time out value
 *
 * Return object pointer if success, else NULL
 */
P_WCN_CORE_DUMP_T wcn_core_dump_init(UINT32 timeout)
{
#define KBYTES (1024*sizeof(char))
#define L1_BUF_SIZE (32*KBYTES)
#define L2_BUF_SIZE (512*KBYTES)

	P_WCN_CORE_DUMP_T core_dmp = NULL;

	core_dmp = (P_WCN_CORE_DUMP_T) osal_malloc(sizeof(WCN_CORE_DUMP_T));
	if (!core_dmp) {
		STP_DBG_ERR_FUNC("alloc mem failed!\n");
		goto fail;
	}

	osal_memset(core_dmp, 0, sizeof(WCN_CORE_DUMP_T));

	core_dmp->compressor =
	    wcn_compressor_init("core_dump_compressor", L1_BUF_SIZE, L2_BUF_SIZE);
	if (!core_dmp->compressor) {
		STP_DBG_ERR_FUNC("create compressor failed!\n");
		goto fail;
	}
	wcn_compressor_reset(core_dmp->compressor, 1, GZIP);

	core_dmp->dmp_timer.timeoutHandler = core_dump_timeout_handler;
	core_dmp->dmp_timer.timeroutHandlerData = (unsigned long) core_dmp;
	osal_timer_create(&core_dmp->dmp_timer);
	core_dmp->timeout = timeout;

	osal_sleepable_lock_init(&core_dmp->dmp_lock);

	core_dmp->sm = CORE_DUMP_INIT;
	STP_DBG_INFO_FUNC("create coredump object OK!\n");

	return core_dmp;

 fail:
	if (core_dmp && core_dmp->compressor) {
		wcn_compressor_deinit(core_dmp->compressor);
		core_dmp->compressor = NULL;
	}

	if (core_dmp) {
		osal_free(core_dmp);
	}

	osal_sleepable_lock_deinit(&core_dmp->dmp_lock);

	return NULL;
}


/* wcn_core_dump_deinit - destroy core dump object
 * @ dmp - pointer of object
 *
 * Retunr 0 if success, else error code
 */
INT32 wcn_core_dump_deinit(P_WCN_CORE_DUMP_T dmp)
{
	if (dmp && dmp->compressor) {
		wcn_compressor_deinit(dmp->compressor);
		dmp->compressor = NULL;
	}

	if (dmp) {
		if (NULL != dmp->p_head) {
			osal_free(dmp->p_head);
			dmp->p_head = NULL;
		}
		osal_sleepable_lock_deinit(&dmp->dmp_lock);
		osal_timer_stop(&dmp->dmp_timer);
		osal_free(dmp);
	}

	return 0;
}


static INT32 wcn_core_dump_check_end(PUINT8 buf, INT32 len)
{
	if (strnstr(buf, "coredump end", len)) {
		return 1;
	} else {
		return 0;
	}
}


/* wcn_core_dump_in - add a packet to compressor buffer
 * @ dmp - pointer of object
 * @ buf - input buffer
 * @ len - data length
 *
 * Retunr 0 if success; return 1 if find end string; else error code
 */
INT32 wcn_core_dump_in(P_WCN_CORE_DUMP_T dmp, PUINT8 buf, INT32 len)
{
	INT32 ret = 0;
	INT32 tmp;
#define MAX_HEAD_LEN 512

	if ((!dmp) || (!buf)) {
		STP_DBG_ERR_FUNC("invalid pointer!\n");
		return -1;
	}

	ret = osal_lock_sleepable_lock(&dmp->dmp_lock);
	if (ret) {
		STP_DBG_ERR_FUNC("--->lock dmp->dmp_lock failed, ret=%d\n", ret);
		return ret;
	}

	switch (dmp->sm) {
	case CORE_DUMP_INIT:
		wcn_compressor_reset(dmp->compressor, 1, GZIP);
		osal_timer_start(&dmp->dmp_timer, STP_CORE_DUMP_TIMEOUT);

		dmp->head_len = 0;
		if (NULL == dmp->p_head) {
			dmp->p_head = osal_malloc(MAX_HEAD_LEN);
			if (NULL == dmp->p_head) {
				STP_DBG_ERR_FUNC
				    ("alloc memory for head information failed, this may cause owner dispatch abnormal\n");
			}
		}
		if (NULL != dmp->p_head) {
			osal_memset(dmp->p_head, 0, MAX_HEAD_LEN);
			(dmp->p_head)[MAX_HEAD_LEN - 1] = '\n';
		}
		/* show coredump start info on UI */
		/* osal_dbg_assert_aee("MT662x f/w coredump start", "MT662x firmware coredump start"); */
#if WMT_PLAT_ALPS
		aee_kernel_dal_show("COMBO_CONSYS coredump start, please wait up to 5 minutes.\n");
#endif
		/* parsing data, and check end srting */
		ret = wcn_core_dump_check_end(buf, len);
		if (ret == 1) {
			STP_DBG_INFO_FUNC("core dump end!\n");
			dmp->sm = CORE_DUMP_DONE;
			wcn_compressor_in(dmp->compressor, buf, len, 1);
		} else {
			dmp->sm = CORE_DUMP_DOING;
			wcn_compressor_in(dmp->compressor, buf, len, 0);
		}
		break;

	case CORE_DUMP_DOING:
		/* parsing data, and check end srting */
		ret = wcn_core_dump_check_end(buf, len);
		if (ret == 1) {
			STP_DBG_INFO_FUNC("core dump end!\n");
			dmp->sm = CORE_DUMP_DONE;
			wcn_compressor_in(dmp->compressor, buf, len, 1);
		} else {
			dmp->sm = CORE_DUMP_DOING;
			wcn_compressor_in(dmp->compressor, buf, len, 0);
		}
		break;

	case CORE_DUMP_DONE:
		wcn_compressor_reset(dmp->compressor, 1, GZIP);
		/*osal_timer_start(&dmp->dmp_timer, STP_CORE_DUMP_TIMEOUT); */
		osal_timer_stop(&dmp->dmp_timer);
		wcn_compressor_in(dmp->compressor, buf, len, 0);
		dmp->sm = CORE_DUMP_DOING;
		break;

	case CORE_DUMP_TIMEOUT:
		break;
	default:
		break;
	}

	if ((NULL != dmp->p_head) && (dmp->head_len < (MAX_HEAD_LEN - 1))) {
		tmp =
		    (dmp->head_len + len) >
		    (MAX_HEAD_LEN - 1) ? (MAX_HEAD_LEN - 1 - dmp->head_len) : len;
		osal_memcpy(dmp->p_head + dmp->head_len, buf, tmp);
		dmp->head_len += tmp;
	}
#if 0
/*move this part to wcn_core_dump_flush, makesure this will always done even if coredump timeout happens*/
	if ((CORE_DUMP_DONE == dmp->sm) || (CORE_DUMP_TIMEOUT == dmp->sm)) {
		ret = _wcn_core_dump_post_handle(dmp);
	}
#endif
	osal_unlock_sleepable_lock(&dmp->dmp_lock);

	return ret;
}


INT32 _wcn_core_dump_post_handle(P_WCN_CORE_DUMP_T dmp)
{
#define INFO_HEAD ";COMBO_CONSYS FW CORE, "
	INT32 ret = 0;
	INT32 tmp = 0;
	ENUM_STP_FW_ISSUE_TYPE issue_type;

	STP_DBG_INFO_FUNC(" enters...\n");
	if ((NULL != dmp->p_head)
	    && (NULL != (osal_strnstr(dmp->p_head, "<ASSERT>", dmp->head_len)))) {
		PINT8 pStr = dmp->p_head;
		PINT8 pDtr = NULL;

		STP_DBG_INFO_FUNC(" <ASSERT> string found\n");
		if (stp_dbg_get_host_trigger_assert())
			issue_type = STP_HOST_TRIGGER_FW_ASSERT;
		else
			issue_type = STP_FW_ASSERT_ISSUE;
		/*parse f/w assert additional informationi for f/w's analysis */
		ret = stp_dbg_set_fw_info(dmp->p_head, dmp->head_len, issue_type);
		if (ret) {
			STP_DBG_ERR_FUNC("set fw issue infor fail(%d),maybe fw warm reset...\n",
					 ret);
			stp_dbg_set_fw_info("Fw Warm reset", osal_strlen("Fw Warm reset"),
					    STP_FW_WARM_RST_ISSUE);
		}
		/* first package, copy to info buffer */
		osal_strcpy(&dmp->info[0], INFO_HEAD);

		/* set f/w assert information to warm reset */
		pStr = osal_strnstr(pStr, "<ASSERT>", dmp->head_len);
		if (NULL != pStr) {
			pDtr = osal_strchr(pStr, '-');
			if (NULL != pDtr) {
				tmp = pDtr - pStr;
				osal_memcpy(&dmp->info[osal_strlen(INFO_HEAD)], pStr, tmp);
				dmp->info[osal_strlen(dmp->info) + 1] = '\0';
			} else {
				tmp = STP_CORE_DUMP_INFO_SZ - osal_strlen(INFO_HEAD);
				tmp = (dmp->head_len > tmp) ? tmp : dmp->head_len;
				osal_memcpy(&dmp->info[osal_strlen(INFO_HEAD)], pStr, tmp);
				dmp->info[STP_CORE_DUMP_INFO_SZ] = '\0';
			}
		}
	}
	else if((NULL != dmp->p_head) && (NULL != (osal_strnstr(dmp->p_head,"ABT", dmp->head_len))))
	{
		STP_DBG_ERR_FUNC("fw ABT happens, set to Fw ABT Exception\n");
		stp_dbg_set_fw_info("Fw ABT Exception", osal_strlen("Fw ABT Exception"), STP_FW_ABT);
		osal_strcpy(&dmp->info[0], INFO_HEAD);
		osal_memcpy(&dmp->info[osal_strlen(INFO_HEAD)], "Fw ABT Exception...", osal_strlen("Fw ABT Exception..."));
		dmp->info[osal_strlen(INFO_HEAD) + osal_strlen("Fw ABT Exception...") + 1] = '\0';
	} else {
		STP_DBG_INFO_FUNC(" <ASSERT> string not found, dmp->head_len:%d\n", dmp->head_len);
		if (NULL == dmp->p_head) {
			STP_DBG_INFO_FUNC(" dmp->p_head is NULL\n");
		} else {
			STP_DBG_INFO_FUNC(" dmp->p_head:%s\n", dmp->p_head);
		}

		/* first package, copy to info buffer */
		osal_strcpy(&dmp->info[0], INFO_HEAD);
		/* set f/w assert information to warm reset */
		osal_memcpy(&dmp->info[osal_strlen(INFO_HEAD)], "Fw warm reset exception...",
			    osal_strlen("Fw warm reset exception..."));
		dmp->info[osal_strlen(INFO_HEAD) + osal_strlen("Fw warm reset exception...") + 1] =
		    '\0';

	}
	dmp->head_len = 0;

	/*set host trigger assert flag to 0 */
	stp_dbg_set_host_assert_info(0, 0, 0);
#if 0
	if (NULL != dmp->p_head) {
		osal_free(dmp->p_head);
		dmp->p_head = NULL;
	}
#endif
	/*set ret value to notify upper layer do dump flush operation */
	ret = 1;
	STP_DBG_INFO_FUNC(" exits...\n");
	return ret;
}


/* wcn_core_dump_out - get compressed data from compressor buffer
 * @ dmp - pointer of object
 * @ pbuf - target buffer's pointer
 * @ len - data length
 *
 * Retunr 0 if success;  else error code
 */
INT32 wcn_core_dump_out(P_WCN_CORE_DUMP_T dmp, PPUINT8 pbuf, PINT32 plen)
{
	INT32 ret = 0;

	if ((!dmp) || (!pbuf) || (!plen)) {
		STP_DBG_ERR_FUNC("invalid pointer!\n");
		return -1;
	}

	ret = osal_lock_sleepable_lock(&dmp->dmp_lock);
	if (ret) {
		STP_DBG_ERR_FUNC("--->lock dmp->dmp_lock failed, ret=%d\n", ret);
		return ret;
	}

	ret = wcn_compressor_out(dmp->compressor, pbuf, plen);

	osal_unlock_sleepable_lock(&dmp->dmp_lock);

	return ret;
}


/* wcn_core_dump_reset - reset core dump sys
 * @ dmp - pointer of object
 * @ timeout - core dump time out value
 *
 * Retunr 0 if success, else error code
 */
INT32 wcn_core_dump_reset(P_WCN_CORE_DUMP_T dmp, UINT32 timeout)
{
	if (!dmp) {
		STP_DBG_ERR_FUNC("invalid pointer!\n");
		return -1;
	}

	dmp->sm = CORE_DUMP_INIT;
	dmp->timeout = timeout;
	osal_timer_stop(&dmp->dmp_timer);
	wcn_compressor_reset(dmp->compressor, 1, GZIP);
	osal_memset(dmp->info, 0, STP_CORE_DUMP_INFO_SZ + 1);

	return 0;
}


/* wcn_core_dump_flush - Fulsh dump data and reset core dump sys
 *
 * Retunr 0 if success, else error code
 */
INT32 wcn_core_dump_flush(INT32 rst)
{
	PUINT8 pbuf = NULL;
	INT32 len = 0;

	if (!g_core_dump) {
		STP_DBG_ERR_FUNC("invalid pointer!\n");
		return -1;
	}

	osal_lock_sleepable_lock(&g_core_dump->dmp_lock);
	_wcn_core_dump_post_handle(g_core_dump);
	osal_unlock_sleepable_lock(&g_core_dump->dmp_lock);

	wcn_core_dump_out(g_core_dump, &pbuf, &len);
	STP_DBG_INFO_FUNC("buf 0x%p, len %d\n", pbuf, len);

	/* show coredump end info on UI */
	/* osal_dbg_assert_aee("MT662x f/w coredump end", "MT662x firmware coredump ends"); */
#if WMT_PLAT_ALPS
	/* call AEE driver API */
	aee_kernel_dal_show("COMBO_CONSYS coredump end\n");
	aed_combo_exception(NULL, 0, (const PINT32)pbuf, len, (const PINT8)g_core_dump->info);
#endif
	/* reset */
	wcn_core_dump_reset(g_core_dump, STP_CORE_DUMP_TIMEOUT);

	return 0;
}


static INT32 wcn_gzip_compressor(PVOID worker, PUINT8 in_buf, INT32 in_sz, PUINT8 out_buf,
				 PINT32 out_sz, INT32 finish)
{
	INT32 ret = 0;
	z_stream *stream = NULL;
	INT32 tmp = *out_sz;

	STP_DBG_INFO_FUNC("in buf 0x%p, in sz %d\n", in_buf, in_sz);
	STP_DBG_INFO_FUNC("out buf 0x%p, out sz %d\n", out_buf, tmp);

	stream = (z_stream *) worker;
	if (!stream) {
		STP_DBG_ERR_FUNC("invalid workspace!\n");
		return -1;
	}

	if (in_sz > 0) {
#if 0
		ret = zlib_deflateReset(stream);
		if (ret != Z_OK) {
			STP_DBG_ERR_FUNC("reset failed!\n");
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
				if (val == Z_OK) {
					continue;
				} else if (val == Z_STREAM_END) {
					break;
				} else {
					STP_DBG_ERR_FUNC("finish operation failed %d\n", val);
					return -3;
				}
			}
		}

		*out_sz = tmp - stream->avail_out;
	}

	STP_DBG_INFO_FUNC("out buf 0x%p, out sz %d\n", out_buf, *out_sz);

	return ret;
}


/* wcn_compressor_init - create a compressor and do init
 * @ name - compressor's name
 * @ L1_buf_sz - L1 buffer size
 * @ L2_buf_sz - L2 buffer size
 *
 * Retunr object's pointer if success, else NULL
 */
P_WCN_COMPRESSOR_T wcn_compressor_init(PUINT8 name, INT32 L1_buf_sz, INT32 L2_buf_sz)
{
	z_stream *pstream = NULL;
	P_WCN_COMPRESSOR_T compress = NULL;

	compress = (P_WCN_COMPRESSOR_T) osal_malloc(sizeof(WCN_COMPRESSOR_T));
	if (!compress) {
		STP_DBG_ERR_FUNC("alloc compressor failed!\n");
		goto fail;
	}

	osal_memset(compress, 0, sizeof(WCN_COMPRESSOR_T));
	osal_memcpy(compress->name, name, STP_OJB_NAME_SZ);

	compress->f_compress_en = 0;
	compress->compress_type = GZIP;

	if (compress->compress_type == GZIP) {
		compress->worker = osal_malloc(sizeof(z_stream));
		if (!compress->worker) {
			STP_DBG_ERR_FUNC("alloc stream failed!\n");
			goto fail;
		}
		pstream = (z_stream *) compress->worker;

		pstream->workspace =
		    osal_malloc(zlib_deflate_workspacesize(MAX_WBITS, MAX_MEM_LEVEL));
		if (!pstream->workspace) {
			STP_DBG_ERR_FUNC("alloc workspace failed!\n");
			goto fail;
		}
		zlib_deflateInit2(pstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
				  DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	}

	compress->handler = wcn_gzip_compressor;
	compress->L1_buf_sz = L1_buf_sz;
	compress->L2_buf_sz = L2_buf_sz;
	compress->L1_pos = 0;
	compress->L2_pos = 0;
	compress->uncomp_size = 0;
	compress->crc32 = 0xffffffff;

	compress->L1_buf = osal_malloc(compress->L1_buf_sz);
	if (!compress->L1_buf) {
		STP_DBG_ERR_FUNC("alloc %d bytes for L1 buf failed!\n", compress->L1_buf_sz);
		goto fail;
	}

	compress->L2_buf = osal_malloc(compress->L2_buf_sz);
	if (!compress->L2_buf) {
		STP_DBG_ERR_FUNC("alloc %d bytes for L2 buf failed!\n", compress->L2_buf_sz);
		goto fail;
	}

	STP_DBG_INFO_FUNC("create compressor OK! L1 %d bytes, L2 %d bytes\n", L1_buf_sz, L2_buf_sz);
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

	STP_DBG_ERR_FUNC("init failed!\n");

	return NULL;
}


/* wcn_compressor_deinit - distroy a compressor
 * @ cprs - compressor's pointer
 *
 * Retunr 0 if success, else NULL
 */
INT32 wcn_compressor_deinit(P_WCN_COMPRESSOR_T cprs)
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

	STP_DBG_INFO_FUNC("destroy OK\n");

	return 0;
}


/* wcn_compressor_in - put in a raw data, and compress L1 buffer if need
 * @ cprs - compressor's pointer
 * @ buf - raw data buffer
 * @ len - raw data length
 * @ finish - core dump finish or not, 1: finished; 0: not finish
 *
 * Retunr 0 if success, else NULL
 */
INT32 wcn_compressor_in(P_WCN_COMPRESSOR_T cprs, PUINT8 buf, INT32 len, INT32 finish)
{
	INT32 tmp_len = 0;
	INT32 ret = 0;

	if (!cprs) {
		STP_DBG_ERR_FUNC("invalid para!\n");
		return -1;
	}

	cprs->uncomp_size += len;

	/* check L1 buf valid space */
	if (len > (cprs->L1_buf_sz - cprs->L1_pos)) {
		STP_DBG_INFO_FUNC("L1 buffer full\n");

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

				if (finish) {
					/* Add 8 byte suffix
					   ===
					   32 bits UNCOMPRESS SIZE
					   32 bits CRC
					 */
					*(uint32_t *) (&cprs->L2_buf[cprs->L2_pos]) =
					    (cprs->crc32 ^ 0xffffffff);
					*(uint32_t *) (&cprs->L2_buf[cprs->L2_pos + 4]) =
					    cprs->uncomp_size;
					cprs->L2_pos += 8;
				}
				STP_DBG_INFO_FUNC("compress OK!\n");
			} else {
				STP_DBG_ERR_FUNC("compress error!\n");
			}
		} else {
			/* no need compress */
			/* Flush L1 buffer to L2 buffer */
			STP_DBG_INFO_FUNC("No need do compress, Put to L2 buf\n");

			tmp_len = cprs->L2_buf_sz - cprs->L2_pos;
			tmp_len = (cprs->L1_pos > tmp_len) ? tmp_len : cprs->L1_pos;
			osal_memcpy(&cprs->L2_buf[cprs->L2_pos], cprs->L1_buf, tmp_len);
			cprs->L2_pos += tmp_len;
		}

		/* reset L1 buf pos */
		cprs->L1_pos = 0;

		/* put curren data to L1 buf */
		if (len > cprs->L1_buf_sz) {
			STP_DBG_ERR_FUNC("len=%d, too long err!\n", len);
		} else {
			STP_DBG_DBG_FUNC("L1 Flushed, and Put %d bytes to L1 buf\n", len);
			osal_memcpy(&cprs->L1_buf[cprs->L1_pos], buf, len);
			cprs->L1_pos += len;
		}
	} else {
		/* put to L1 buffer */
		STP_DBG_DBG_FUNC("Put %d bytes to L1 buf\n", len);

		osal_memcpy(&cprs->L1_buf[cprs->L1_pos], buf, len);
		cprs->L1_pos += len;
	}

	return ret;
}


/* wcn_compressor_out - get the result data from L2 buffer
 * @ cprs - compressor's pointer
 * @ pbuf - point to L2 buffer
 * @ plen - out len
 *
 * Retunr 0 if success, else NULL
 */
INT32 wcn_compressor_out(P_WCN_COMPRESSOR_T cprs, PPUINT8 pbuf, PINT32 plen)
{
	INT32 ret = 0;
	INT32 tmp_len = 0;

	if ((!cprs) || (!pbuf) || (!plen)) {
		STP_DBG_ERR_FUNC("invalid para!\n");
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
				   ===
				   32 bits UNCOMPRESS SIZE
				   32 bits CRC
				 */
				*(uint32_t *) (&cprs->L2_buf[cprs->L2_pos]) =
				    (cprs->crc32 ^ 0xffffffff);
				*(uint32_t *) (&cprs->L2_buf[cprs->L2_pos + 4]) = cprs->uncomp_size;
				cprs->L2_pos += 8;

				STP_DBG_INFO_FUNC("compress OK!\n");
			} else {
				STP_DBG_ERR_FUNC("compress error!\n");
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

	STP_DBG_INFO_FUNC("0x%p, len %d\n", *pbuf, *plen);

#if 1
	ret = zlib_deflateReset((z_stream *) cprs->worker);
	if (ret != Z_OK) {
		STP_DBG_ERR_FUNC("reset failed!\n");
		return -2;
	}
#endif

	return 0;
}


/* wcn_compressor_reset - reset compressor
 * @ cprs - compressor's pointer
 * @ enable - enable/disable compress
 * @ type - compress algorithm
 *
 * Retunr 0 if success, else NULL
 */
INT32 wcn_compressor_reset(P_WCN_COMPRESSOR_T cprs, UINT8 enable, WCN_COMPRESS_ALG_T type)
{
	if (!cprs) {
		STP_DBG_ERR_FUNC("invalid para!\n");
		return -1;
	}

	cprs->f_compress_en = enable;
	/* cprs->f_compress_en = 0; // disable compress for test */
	cprs->compress_type = type;
	cprs->L1_pos = 0;
	cprs->L2_pos = 0;
	cprs->uncomp_size = 0;
	cprs->crc32 = 0xffffffff;

	STP_DBG_INFO_FUNC("OK! compress algorithm %d\n", type);

	return 0;
}

INT32 wcn_core_dump_nl(P_WCN_CORE_DUMP_T dmp, PUINT8 buf, INT32 len)
{
	INT32 ret = 0;

	if ((!dmp) || (!buf)) {
		STP_DBG_ERR_FUNC("invalid pointer!\n");
		return -1;
	}

	ret = osal_lock_sleepable_lock(&dmp->dmp_lock);
	if (ret) {
		STP_DBG_ERR_FUNC("--->lock dmp->dmp_lock failed, ret=%d\n", ret);
		return ret;
	}

	switch (dmp->sm) {
	case CORE_DUMP_INIT:
		osal_timer_start(&dmp->dmp_timer, STP_CORE_DUMP_TIMEOUT);
		STP_DBG_WARN_FUNC("COMBO_CONSYS coredump start, please wait up to 1 minutes.\n");
		/* check end srting */
		ret = wcn_core_dump_check_end(buf, len);
		if (ret == 1) {
			STP_DBG_INFO_FUNC("core dump end!\n");
			osal_timer_stop(&dmp->dmp_timer);
			dmp->sm = CORE_DUMP_INIT;
		} else {
			dmp->sm = CORE_DUMP_DOING;
		}
		break;

	case CORE_DUMP_DOING:
		/* check end srting */
		ret = wcn_core_dump_check_end(buf, len);
		if (ret == 1) {
			STP_DBG_INFO_FUNC("core dump end!\n");
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

	osal_unlock_sleepable_lock(&dmp->dmp_lock);

	return ret;
}


static VOID stp_dbg_dump_data(PUINT8 pBuf, PINT8 title, INT32 len)
{
	INT32 k = 0;
	char str[240] = {""};
	INT32 strp = 0;
	INT32 strlen = 0;
	pr_warn(" %s-len:%d\n", title, len);
	/* pr_warn("    ", title, len); */
	for (k = 0; k < len; k++) {
		if (strp < 200) {
			strlen = osal_sprintf(&str[strp], "0x%02x ", pBuf[k]);
			strp += strlen;
		} else {
			pr_warn("More than 200 of the data is too much\n");
			break;
		}
	}
	osal_sprintf(&str[strp], "--end\n");
	pr_warn("%s", str);
}


static INT32 _stp_dbg_enable(MTKSTP_DBG_T *stp_dbg)
{

	unsigned long flags;

	spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);
	stp_dbg->pkt_trace_no = 0;
	stp_dbg->is_enable = 1;
	spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);

	return 0;
}

static INT32 _stp_dbg_disable(MTKSTP_DBG_T *stp_dbg)
{

	unsigned long flags;

	spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);
	stp_dbg->pkt_trace_no = 0;
	stp_dbg->is_enable = 0;
	spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);

	return 0;
}

static INT32 _stp_dbg_dmp_in(MTKSTP_DBG_T *stp_dbg, PINT8 buf, INT32 len)
{

	unsigned long flags;
	UINT32 internalFlag = stp_dbg->logsys->size < STP_DBG_LOG_ENTRY_NUM;
	UINT32 in_len = 0;
	/* #ifdef CONFIG_LOG_STP_INTERNAL */
	/* Here we record log in this circle buffer, if buffer is full , select to overlap earlier log, logic should be okay */
	internalFlag = 1;
	/* #endif */
	spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);

	if (internalFlag) {
		in_len = (len >= STP_DBG_LOG_ENTRY_SZ)? (STP_DBG_LOG_ENTRY_SZ):(len);
		stp_dbg->logsys->queue[stp_dbg->logsys->in].id = 0;
        stp_dbg->logsys->queue[stp_dbg->logsys->in].len = in_len;
        //memset(&(stp_dbg->logsys->queue[stp_dbg->logsys->in].buffer[0]),0, STP_DBG_LOG_ENTRY_SZ);
		memcpy(&(stp_dbg->logsys->queue[stp_dbg->logsys->in].buffer[0]),
		       buf, ((len >= STP_DBG_LOG_ENTRY_SZ) ? (STP_DBG_LOG_ENTRY_SZ) : (len)));

		stp_dbg->logsys->size++;

		stp_dbg->logsys->size =
		    (stp_dbg->logsys->size >
		     STP_DBG_LOG_ENTRY_NUM) ? STP_DBG_LOG_ENTRY_NUM : stp_dbg->logsys->size;

		if (0 != gStpDbgLogOut) {
			STP_DBG_HDR_T *pHdr = NULL;
			PINT8 pBuf = NULL;
			UINT32 len = 0;
			pHdr =
			    (STP_DBG_HDR_T *) &(stp_dbg->logsys->queue[stp_dbg->logsys->in].
						 buffer[0]);
			pBuf =
			    (PINT8)&(stp_dbg->logsys->queue[stp_dbg->logsys->in].buffer[0]) +
			    sizeof(STP_DBG_HDR_T);
			len =
			    stp_dbg->logsys->queue[stp_dbg->logsys->in].len - sizeof(STP_DBG_HDR_T);
			pr_warn("STP-DBG:%d.%ds, %s:pT%sn(%d)l(%d)s(%d)a(%d)\n", pHdr->sec,
				pHdr->usec, pHdr->dir == PKT_DIR_TX ? "Tx" : "Rx",
				gStpDbgType[pHdr->type], pHdr->no, pHdr->len, pHdr->seq, pHdr->ack);
			if (0 < len) {
				stp_dbg_dump_data(pBuf, pHdr->dir == PKT_DIR_TX ? "Tx" : "Rx", len);
			}

		}
		stp_dbg->logsys->in =
		    (stp_dbg->logsys->in >=
		     (STP_DBG_LOG_ENTRY_NUM - 1)) ? (0) : (stp_dbg->logsys->in + 1);
		STP_DBG_DBG_FUNC("logsys size = %d, in = %d\n", stp_dbg->logsys->size,
				 stp_dbg->logsys->in);
	} else {
		STP_DBG_WARN_FUNC("logsys FULL!\n");
	}

	spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);

	return 0;
}

INT32 stp_gdb_notify_btm_dmp_wq(MTKSTP_DBG_T *stp_dbg)
{
	INT32 retval = 0;
/* #ifndef CONFIG_LOG_STP_INTERNAL */

	if (stp_dbg->btm != NULL) {
		retval += stp_btm_notify_wmt_dmp_wq((MTKSTP_BTM_T *) stp_dbg->btm);
	}
/* #endif */

	return retval;
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

INT32 stp_dbg_dmp_in(MTKSTP_DBG_T *stp_dbg, PINT8 buf, INT32 len)
{
	return _stp_dbg_dmp_in(stp_dbg, buf, len);
}

INT32 stp_dbg_dmp_print(MTKSTP_DBG_T *stp_dbg)
{
#define MAX_DMP_NUM 80
	unsigned long flags;
	PINT8 pBuf = NULL;
	INT32 len = 0;
	STP_DBG_HDR_T *pHdr = NULL;
	UINT32 dumpSize = 0;
	UINT32 inIndex = 0;
	UINT32 outIndex = 0;
	spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);
	/* Not to dequeue from loging system */
	inIndex = stp_dbg->logsys->in;
	dumpSize = stp_dbg->logsys->size;
	if (STP_DBG_LOG_ENTRY_NUM == dumpSize) {
		outIndex = inIndex;
	} else {
		outIndex = ((inIndex + STP_DBG_LOG_ENTRY_NUM) - dumpSize) % STP_DBG_LOG_ENTRY_NUM;
	}

	if (dumpSize > MAX_DMP_NUM) {

		outIndex += (dumpSize - MAX_DMP_NUM);
		outIndex %= STP_DBG_LOG_ENTRY_NUM;
		dumpSize = MAX_DMP_NUM;

	}
	STP_DBG_INFO_FUNC("loged packet size = %d, in(%d), out(%d)\n", dumpSize, inIndex, outIndex);
	while (dumpSize > 0) {
		pHdr = (STP_DBG_HDR_T *) &(stp_dbg->logsys->queue[outIndex].buffer[0]);
		pBuf = &(stp_dbg->logsys->queue[outIndex].buffer[0]) + sizeof(STP_DBG_HDR_T);
		len = stp_dbg->logsys->queue[outIndex].len - sizeof(STP_DBG_HDR_T);
		len = len > STP_PKT_SZ ? STP_PKT_SZ : len;
		pr_warn("STP-DBG:%d.%ds, %s:pT%sn(%d)l(%d)s(%d)a(%d)\n",
			pHdr->sec,
			pHdr->usec,
			pHdr->dir == PKT_DIR_TX ? "Tx" : "Rx",
			gStpDbgType[pHdr->type], pHdr->no, pHdr->len, pHdr->seq, pHdr->ack);


		if (0 < len) {
			stp_dbg_dump_data(pBuf, pHdr->dir == PKT_DIR_TX ? "Tx" : "Rx", len);
		}
		outIndex = (outIndex >= (STP_DBG_LOG_ENTRY_NUM - 1)) ? (0) : (outIndex + 1);
		dumpSize--;

	}

	spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);

	return 0;
}

INT32 stp_dbg_dmp_out_ex(PINT8 buf, PINT32 len)
{
	return stp_dbg_dmp_out(g_stp_dbg, buf, len);
}

INT32 stp_dbg_dmp_out(MTKSTP_DBG_T *stp_dbg, PINT8 buf, PINT32 len)
{

	unsigned long flags;
	INT32 remaining = 0;
	*len = 0;
	spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);

	if (stp_dbg->logsys->size > 0) {
		memcpy(buf, &(stp_dbg->logsys->queue[stp_dbg->logsys->out].buffer[0]),
		       stp_dbg->logsys->queue[stp_dbg->logsys->out].len);

		(*len) = stp_dbg->logsys->queue[stp_dbg->logsys->out].len;
		stp_dbg->logsys->out =
		    (stp_dbg->logsys->out >=
		     (STP_DBG_LOG_ENTRY_NUM - 1)) ? (0) : (stp_dbg->logsys->out + 1);
		stp_dbg->logsys->size--;

		STP_DBG_DBG_FUNC("logsys size = %d, out = %d\n", stp_dbg->logsys->size,
				 stp_dbg->logsys->out);
	} else {
		STP_DBG_LOUD_FUNC("logsys EMPTY!\n");
	}

	remaining = (stp_dbg->logsys->size == 0) ? (0) : (1);

	spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);

	return remaining;
}

static INT32 stp_dbg_get_avl_entry_num(MTKSTP_DBG_T*stp_dbg)
{
	osal_bug_on(!stp_dbg);
	if(0 == stp_dbg->logsys->size) {
		return STP_DBG_LOG_ENTRY_NUM;
	} else {
		return (stp_dbg->logsys->in > stp_dbg->logsys->out) ? 
			(STP_DBG_LOG_ENTRY_NUM - stp_dbg->logsys->in + stp_dbg->logsys->out) : 
			(stp_dbg->logsys->out - stp_dbg->logsys->in);
	}
}

static INT32 stp_dbg_fill_hdr(struct stp_dbg_pkt_hdr *hdr, INT32 type, INT32 ack, INT32 seq, INT32 crc, INT32 dir, INT32 len, INT32 dbg_type)
{
	struct timeval now;

	if (!hdr) {
		STP_DBG_ERR_FUNC("function invalid\n");
		return -EINVAL;
	} else {
		do_gettimeofday(&now);
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
		return 0;
	}
}

static INT32 stp_dbg_add_pkt(MTKSTP_DBG_T *stp_dbg, struct stp_dbg_pkt_hdr *hdr, const PUINT8 body)
{
	/* fix the frame size large issues. */
	static struct stp_dbg_pkt stp_pkt;
	UINT32 hdr_sz = sizeof(struct stp_dbg_pkt_hdr);
	UINT32 body_sz = 0;

	osal_bug_on(!stp_dbg);

	if (hdr->dbg_type == STP_DBG_PKT) {
		body_sz = (hdr->len <= STP_PKT_SZ) ? (hdr->len) : (STP_PKT_SZ);
	} else {
		body_sz = (hdr->len <= STP_DMP_SZ) ? (hdr->len) : (STP_DMP_SZ);
	}

	hdr->no = stp_dbg->pkt_trace_no++;
	memcpy((PUINT8) &stp_pkt.hdr, (PUINT8) hdr, hdr_sz);
	if (body != NULL) {
		memcpy((PUINT8) &stp_pkt.raw[0], body, body_sz);
	}
#if 1
	if(hdr->dbg_type == STP_DBG_FW_DMP)
    {
    	if(hdr->last_dbg_type != STP_DBG_FW_DMP)
    	{
    		unsigned long flags;
	    	STP_DBG_INFO_FUNC("reset stp_dbg logsys when queue fw coredump package(%d)\n",hdr->last_dbg_type);
			STP_DBG_INFO_FUNC("dump 1st fw coredump package len(%d) for confirming\n",hdr->len);
			spin_lock_irqsave(&(stp_dbg->logsys->lock), flags);
			stp_dbg->logsys->in = 0;
			stp_dbg->logsys->out = 0;
			stp_dbg->logsys->size = 0;
			spin_unlock_irqrestore(&(stp_dbg->logsys->lock), flags);
    	}else
    	{
    		UINT32 avl_num = stp_dbg_get_avl_entry_num(stp_dbg);
			if(!avl_num)
			{
				STP_DBG_ERR_FUNC("there is no avl entry stp_dbg logsys!!!\n");
			}
    	}
	}
#endif
	
	_stp_dbg_dmp_in(stp_dbg, (PINT8)&stp_pkt, hdr_sz + body_sz);
	/* Only FW DMP MSG should inform BTM-CORE to dump packet to native process */
	if (hdr->dbg_type == STP_DBG_FW_DMP) {
		stp_gdb_notify_btm_dmp_wq(stp_dbg);
	}

	return 0;
}

INT32 stp_dbg_log_pkt(MTKSTP_DBG_T *stp_dbg, INT32 dbg_type,
		    INT32 type, INT32 ack_no, INT32 seq_no, INT32 crc, INT32 dir, INT32 len,
		    const PUINT8 body)
{

	struct stp_dbg_pkt_hdr hdr;

	if (stp_dbg->is_enable == 0) {
		/*dbg is disable,and not to log */
	} else {
		stp_dbg_fill_hdr(&hdr,
				 (INT32)type,
				 (INT32)ack_no,
				 (INT32)seq_no, (INT32)crc, (INT32)dir, (INT32)len, (INT32)dbg_type);

		stp_dbg_add_pkt(stp_dbg, &hdr, body);
	}

	return 0;
}

INT32 stp_dbg_enable(MTKSTP_DBG_T *stp_dbg)
{
	return _stp_dbg_enable(stp_dbg);
}

INT32 stp_dbg_disable(MTKSTP_DBG_T *stp_dbg)
{
	return _stp_dbg_disable(stp_dbg);
}

static VOID stp_dbg_nl_init(VOID)
{
	if (genl_register_family(&stp_dbg_gnl_family) != 0) {
		STP_DBG_ERR_FUNC("%s(): GE_NELINK family registration fail\n", __func__);
	} else {
		if (genl_register_ops(&stp_dbg_gnl_family, &stp_dbg_gnl_ops_bind) != 0) {
			STP_DBG_ERR_FUNC("%s(): BIND operation registration fail\n", __func__);
		}

		if (genl_register_ops(&stp_dbg_gnl_family, &stp_dbg_gnl_ops_reset) != 0) {
			STP_DBG_ERR_FUNC("%s(): RESET operation registration fail\n", __func__);
		}
	}

	return;
}

static VOID stp_dbg_nl_deinit(VOID)
{
	genl_unregister_family(&stp_dbg_gnl_family);
	return;
}

static INT32 stp_dbg_nl_bind(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *na;
	PINT8 mydata;

	if (info == NULL) {
		goto out;
	}

	STP_DBG_INFO_FUNC("%s():->\n", __func__);

	na = info->attrs[STP_DBG_ATTR_MSG];

	if (na) {
		mydata = (PINT8)nla_data(na);
	}

	if (num_bind_process < MAX_BIND_PROCESS) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		bind_pid[num_bind_process] = info->snd_pid;
		num_bind_process++;
		STP_DBG_INFO_FUNC("%s():-> pid  = %d\n", __func__, info->snd_pid);
#else
		bind_pid[num_bind_process] = info->snd_portid;
		num_bind_process++;
		STP_DBG_INFO_FUNC("%s():-> pid  = %d\n", __func__, info->snd_portid);
#endif
	} else {
		STP_DBG_ERR_FUNC("%s(): exceeding binding limit %d\n", __func__, MAX_BIND_PROCESS);
	}

 out:
	return 0;
}

static INT32 stp_dbg_nl_reset(struct sk_buff *skb, struct genl_info *info)
{
	STP_DBG_ERR_FUNC("%s(): should not be invoked\n", __func__);

	return 0;
}

INT32 stp_dbg_nl_send(PINT8 aucMsg, UINT8 cmd, INT32 len)
{
	struct sk_buff *skb = NULL;
	PVOID msg_head = NULL;
	INT32 rc = -1;
	INT32 i;
	INT32 ret = 0;

	if (num_bind_process == 0) {
		/* no listening process */
		STP_DBG_ERR_FUNC("%s(): the process is not invoked\n", __func__);
		return 0;
	}

	ret = wcn_core_dump_nl(g_core_dump, aucMsg, len);
	if (ret < 0)
		return ret;
	if (ret == 32) {
		return ret;
	}

	for (i = 0; i < num_bind_process; i++) {
		skb = genlmsg_new(2048, GFP_KERNEL);

		if (skb) {
			msg_head =
			    genlmsg_put(skb, 0, stp_dbg_seqnum++, &stp_dbg_gnl_family, 0, cmd);
			if (msg_head == NULL) {
				nlmsg_free(skb);
				STP_DBG_DBG_FUNC("%s(): genlmsg_put fail...\n", __func__);
				return -1;
			}

			//rc = nla_put_string(skb, STP_DBG_ATTR_MSG, aucMsg);
			rc = nla_put(skb, STP_DBG_ATTR_MSG, len, aucMsg);
			if (rc != 0) {
				nlmsg_free(skb);
				STP_DBG_DBG_FUNC("%s(): nla_put_string fail...: %d\n", __func__, rc);
				return rc;
			}

			/* finalize the message */
			genlmsg_end(skb, msg_head);

			/* sending message */
			rc = genlmsg_unicast(&init_net, skb, bind_pid[i]);
			if (rc != 0) {
				STP_DBG_DBG_FUNC("%s(): genlmsg_unicast fail...: %d\n", __func__, rc);
				return rc;
			}
		} else {
			STP_DBG_ERR_FUNC("%s(): genlmsg_new fail...\n", __func__);
			return -1;
		}
	}

	return 0;
}


INT32 stp_dbg_aee_send(PUINT8 aucMsg, INT32 len, INT32 cmd)
{
	INT32 ret = 0;

	/* buffered to compressor */
	ret = wcn_core_dump_in(g_core_dump, aucMsg, len);
	if (ret == 1) {
		wcn_core_dump_flush(0);
	}

	return ret;
}


PUINT8 _stp_dbg_id_to_task(UINT32 id)
{
	PUINT8 taskStr[] = {
		"Task_WMT",
		"Task_BT",
		"Task_Wifi",
		"Task_Tst",
		"Task_FM",
		"Task_Idle",
		"Task_DrvStp",
		"Task_DrvBtif",
		"Task_NatBt",
	};
	return taskStr[id];
}


INT32 _stp_dbg_parser_assert_str(PINT8 str, ENUM_ASSERT_INFO_PARSER_TYPE type)
{
	PINT8 pStr = NULL;
	PINT8 pDtr = NULL;
	PINT8 pTemp = NULL;
	PINT8 pTemp2 = NULL;
	INT8 tempBuf[64] = { 0 };
	UINT32 len = 0;

	PINT8 parser_sub_string[] = {
		"<ASSERT> ",
		"id=",
		"isr=",
		"irq=",
		"rc="
	};

	if (!str) {
		STP_DBG_ERR_FUNC("NULL string source\n");
		return -1;
	}

	if (!g_stp_dbg_cpupcr) {
		STP_DBG_ERR_FUNC("NULL pointer\n");
		return -2;
	}

	pStr = str;
	STP_DBG_DBG_FUNC("source infor:%s\n", pStr);

	switch (type) {
	case STP_DBG_ASSERT_INFO:
		pDtr = osal_strstr(pStr, parser_sub_string[type]);

		if (NULL != pDtr) {
			pDtr += osal_strlen(parser_sub_string[type]);
			pTemp = osal_strchr(pDtr, ' ');
		} else {
			STP_DBG_ERR_FUNC("parser str is NULL,substring(%s)\n",
					 parser_sub_string[type]);
			return -3;
		}

		len = pTemp - pDtr;
		osal_memcpy(&g_stp_dbg_cpupcr->assert_info[0], "assert@", osal_strlen("assert@"));
		osal_memcpy(&g_stp_dbg_cpupcr->assert_info[osal_strlen("assert@")], pDtr, len);
		g_stp_dbg_cpupcr->assert_info[osal_strlen("assert@") + len] = '_';

		pTemp = osal_strchr(pDtr, '#');
		pTemp += 1;

		pTemp2 = osal_strchr(pTemp, ' ');
		osal_memcpy(&g_stp_dbg_cpupcr->assert_info[osal_strlen("assert@") + len + 1], pTemp,
			    pTemp2 - pTemp);
		g_stp_dbg_cpupcr->assert_info[osal_strlen("assert@") + len + 1 + pTemp2 - pTemp] =
		    '\0';
		STP_DBG_INFO_FUNC("assert info:%s\n", &g_stp_dbg_cpupcr->assert_info[0]);
		break;

	case STP_DBG_FW_TASK_ID:
		pDtr = osal_strstr(pStr, parser_sub_string[type]);

		if (NULL != pDtr) {
			pDtr += osal_strlen(parser_sub_string[type]);
			pTemp = osal_strchr(pDtr, ' ');
		} else {
			STP_DBG_ERR_FUNC("parser str is NULL,substring(%s)\n",
					 parser_sub_string[type]);
			return -3;
		}

		len = pTemp - pDtr;
		osal_memcpy(&tempBuf[0], pDtr, len);
		tempBuf[len] = '\0';
		g_stp_dbg_cpupcr->fwTaskId = osal_strtol(tempBuf, NULL, 16);

		STP_DBG_INFO_FUNC("fw task id :%x\n", (UINT32) osal_strtol(tempBuf, NULL, 16));
		break;

	case STP_DBG_FW_ISR:
		pDtr = osal_strstr(pStr, parser_sub_string[type]);

		if (NULL != pDtr) {
			pDtr += osal_strlen(parser_sub_string[type]);
			pTemp = osal_strchr(pDtr, ',');
		} else {
			STP_DBG_ERR_FUNC("parser str is NULL,substring(%s)\n",
					 parser_sub_string[type]);
			return -3;
		}

		len = pTemp - pDtr;
		osal_memcpy(&tempBuf[0], pDtr, len);
		tempBuf[len] = '\0';

		g_stp_dbg_cpupcr->fwIsr = osal_strtol(tempBuf, NULL, 16);

		STP_DBG_INFO_FUNC("fw isr str:%x\n", (UINT32) osal_strtol(tempBuf, NULL, 16));
		break;

	case STP_DBG_FW_IRQ:
		pDtr = osal_strstr(pStr, parser_sub_string[type]);

		if (NULL != pDtr) {
			pDtr += osal_strlen(parser_sub_string[type]);
			pTemp = osal_strchr(pDtr, ',');
		} else {
			STP_DBG_ERR_FUNC("parser str is NULL,substring(%s)\n",
					 parser_sub_string[type]);
			return -3;
		}

		len = pTemp - pDtr;
		osal_memcpy(&tempBuf[0], pDtr, len);
		tempBuf[len] = '\0';
		g_stp_dbg_cpupcr->fwRrq = osal_strtol(tempBuf, NULL, 16);

		STP_DBG_INFO_FUNC("fw irq value:%x\n", (UINT32) osal_strtol(tempBuf, NULL, 16));
		break;

	case STP_DBG_ASSERT_TYPE:
		pDtr = osal_strstr(pStr, parser_sub_string[type]);

		if (NULL != pDtr) {
			pDtr += osal_strlen(parser_sub_string[type]);
			pTemp = osal_strchr(pDtr, ',');
		} else {
			STP_DBG_ERR_FUNC("parser str is NULL,substring(%s)\n",
					 parser_sub_string[type]);
			return -3;
		}

		len = pTemp - pDtr;
		osal_memcpy(&tempBuf[0], pDtr, len);
		tempBuf[len] = '\0';

		if (0 == osal_memcmp(tempBuf, "*", osal_strlen("*"))) {
			osal_memcpy(&g_stp_dbg_cpupcr->assert_type[0], "general assert",
				    osal_strlen("general assert"));
		}

		if (0 ==
		    osal_memcmp(tempBuf, "Watch Dog Timeout", osal_strlen("Watch Dog Timeout"))) {
			osal_memcpy(&g_stp_dbg_cpupcr->assert_type[0], "wdt", osal_strlen("wdt"));
		}

		if (0 == osal_memcmp(tempBuf, "RB_FULL", osal_strlen("RB_FULL"))) {
			osal_memcpy(&g_stp_dbg_cpupcr->assert_type[0], tempBuf, len);

			pDtr = osal_strstr(&g_stp_dbg_cpupcr->assert_type[0], "RB_FULL(");

			if (NULL != pDtr) {
				pDtr += osal_strlen("RB_FULL(");
				pTemp = osal_strchr(pDtr, ')');
			} else {
				STP_DBG_ERR_FUNC("parser str is NULL,substring(RB_FULL()\n");
				return -4;
			}

			len = pTemp - pDtr;
			osal_memcpy(&tempBuf[0], pDtr, len);
			tempBuf[len] = '\0';

			g_stp_dbg_cpupcr->fwTaskId = osal_strtol(tempBuf, NULL, 16);

			STP_DBG_INFO_FUNC("update fw task id :%x\n",
					  (UINT32) osal_strtol(tempBuf, NULL, 16));
		}

		STP_DBG_INFO_FUNC("fw asert type:%s\n", g_stp_dbg_cpupcr->assert_type);
		break;

	default:
		STP_DBG_ERR_FUNC("unknow parser type\n");
		break;
	}

	return 0;
}


P_STP_DBG_CPUPCR_T stp_dbg_cpupcr_init(VOID)
{
	P_STP_DBG_CPUPCR_T pSdCpupcr = NULL;

	pSdCpupcr = (P_STP_DBG_CPUPCR_T) osal_malloc(osal_sizeof(STP_DBG_CPUPCR_T));

	if (!pSdCpupcr) {
		STP_DBG_ERR_FUNC("stp dbg cpupcr allocate memory fail!\n");
		return NULL;
	}

	osal_memset(pSdCpupcr, 0, osal_sizeof(STP_DBG_CPUPCR_T));

	osal_sleepable_lock_init(&pSdCpupcr->lock);

	return pSdCpupcr;
}


VOID stp_dbg_cpupcr_deinit(P_STP_DBG_CPUPCR_T pCpupcr)
{
	if (pCpupcr) {
		osal_sleepable_lock_deinit(&pCpupcr->lock);
		osal_free(pCpupcr);
		pCpupcr = NULL;
	}
}


INT32 stp_dbg_set_version_info(UINT32 chipid, PUINT8 pRomVer, PUINT8 wifiVer, PUINT8 pPatchVer,
			       PUINT8 pPatchBrh)
{
	if (g_stp_dbg_cpupcr) {
		osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
		g_stp_dbg_cpupcr->chipId = chipid;

		if (wifiVer) {
			osal_memcpy(g_stp_dbg_cpupcr->wifiVer, wifiVer, 4);
		}

		if (pRomVer) {
			osal_memcpy(g_stp_dbg_cpupcr->romVer, pRomVer, 2);
		}

		if (pPatchVer) {
			osal_memcpy(g_stp_dbg_cpupcr->patchVer, pPatchVer, 8);
		}

		if (pPatchBrh) {
			osal_memcpy(g_stp_dbg_cpupcr->branchVer, pPatchBrh, 4);
		}

		osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
	} else {
		STP_DBG_ERR_FUNC("NULL pointer\n");
		return -1;
	}

	STP_DBG_INFO_FUNC("chipid(0x%x),wifiver(%s),romver(%s),patchver(%s),branchver(%s)\n",
			  g_stp_dbg_cpupcr->chipId, g_stp_dbg_cpupcr->wifiVer,
			  &g_stp_dbg_cpupcr->romVer[0], &g_stp_dbg_cpupcr->patchVer[0],
			  &g_stp_dbg_cpupcr->branchVer[0]);
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

UINT32 stp_dbg_get_host_trigger_assert(VOID)
{
	return g_stp_dbg_cpupcr->host_assert_info.assert_from_host;
}

INT32 stp_dbg_set_fw_info(PUINT8 issue_info, UINT32 len, ENUM_STP_FW_ISSUE_TYPE issue_type)
{
	ENUM_ASSERT_INFO_PARSER_TYPE type_index;
	PUINT8 tempbuf = NULL;
	UINT32 i = 0;
	INT32 iRet = 0;

	if (NULL == issue_info) {
		STP_DBG_ERR_FUNC("null issue infor\n");
		return -1;
	}

	g_stp_dbg_cpupcr->issue_type = issue_type;
	osal_memset(&g_stp_dbg_cpupcr->assert_info[0], 0, STP_ASSERT_INFO_SIZE);

	/*print patch version when assert happened */
	STP_DBG_INFO_FUNC("=======================================\n");
	STP_DBG_INFO_FUNC("[combo patch]patch version:%s\n", g_stp_dbg_cpupcr->patchVer);
	STP_DBG_INFO_FUNC("[combo patch]ALPS branch:%s\n", g_stp_dbg_cpupcr->branchVer);
	STP_DBG_INFO_FUNC("=======================================\n");

	if (STP_FW_ASSERT_ISSUE == issue_type || (STP_HOST_TRIGGER_FW_ASSERT == issue_type)) {
		tempbuf = osal_malloc(len);

		if (!tempbuf) {
			return -2;
		}

		osal_memcpy(&tempbuf[0], issue_info, len);

		for (i = 0; i < len; i++) {
			if ((tempbuf[i] == '\0') || (tempbuf[i] == '\n')) {
				tempbuf[i] = '?';
			}
		}
		tempbuf[len] = '\0';
		STP_DBG_INFO_FUNC("tempbuf<%s>\n", tempbuf);
		STP_DBG_INFO_FUNC("issue type(%d)\n", issue_type);


		for (type_index = STP_DBG_ASSERT_INFO; type_index < STP_DBG_PARSER_TYPE_MAX;
		     type_index++) {
			iRet += _stp_dbg_parser_assert_str(&tempbuf[0], type_index);
		}

		if (iRet) {
			STP_DBG_ERR_FUNC("passert assert infor fail(%d)\n", iRet);
		}

		if (STP_HOST_TRIGGER_FW_ASSERT == issue_type) {
			switch (g_stp_dbg_cpupcr->host_assert_info.drv_type) {
			case 0:
				STP_DBG_INFO_FUNC("bt trigger assert\n");
				osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
				if (31 != g_stp_dbg_cpupcr->host_assert_info.reason)
					/*BT firmware trigger assert */
				{
					g_stp_dbg_cpupcr->fwTaskId = 1;

				} else
					/*BT stack trigger assert */
				{
					g_stp_dbg_cpupcr->fwTaskId = 8;
				}

				g_stp_dbg_cpupcr->host_assert_info.assert_from_host = 0;
				/* g_stp_dbg_cpupcr->host_assert_info.drv_type = 0; */
				/* g_stp_dbg_cpupcr->host_assert_info.reason = 0; */

				osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);

				break;
			default:
				break;
			}

		}
		osal_free(tempbuf);
	} else if (STP_FW_NOACK_ISSUE == issue_type) {
		osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
		osal_memcpy(&g_stp_dbg_cpupcr->assert_info[0], issue_info, len);
		g_stp_dbg_cpupcr->fwTaskId = 6;
		g_stp_dbg_cpupcr->fwRrq = 0;
		g_stp_dbg_cpupcr->fwIsr = 0;
		osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
	} else if (STP_DBG_PROC_TEST == issue_type) {
		osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
		osal_memcpy(&g_stp_dbg_cpupcr->assert_info[0], issue_info, len);
		g_stp_dbg_cpupcr->fwTaskId = 0;
		g_stp_dbg_cpupcr->fwRrq = 0;
		g_stp_dbg_cpupcr->fwIsr = 0;
		osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
	} else if (STP_FW_WARM_RST_ISSUE == issue_type) {
		osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
		osal_memcpy(&g_stp_dbg_cpupcr->assert_info[0], issue_info, len);
		g_stp_dbg_cpupcr->fwTaskId = 0;
		g_stp_dbg_cpupcr->fwRrq = 0;
		g_stp_dbg_cpupcr->fwIsr = 0;
		osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
    } else if (STP_FW_ABT == issue_type)
    {
    	osal_lock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
        osal_memcpy(&g_stp_dbg_cpupcr->assert_info[0], issue_info, len);
        g_stp_dbg_cpupcr->fwTaskId = 0;
        g_stp_dbg_cpupcr->fwRrq = 0;
        g_stp_dbg_cpupcr->fwIsr = 0;
        osal_unlock_sleepable_lock(&g_stp_dbg_cpupcr->lock);
	} else {
		STP_DBG_ERR_FUNC("invalid issue type(%d)\n", issue_type);
		return -3;
	}

	return iRet;
}


INT32 stp_dbg_cpupcr_infor_format(PPUINT8 buf, PUINT32 str_len)
{
	UINT32 len = 0;
	UINT32 i = 0;

	if (!g_stp_dbg_cpupcr) {
		STP_DBG_ERR_FUNC("NULL pointer\n");
		return -1;
	}

    STP_DBG_INFO_FUNC("printf assert issue type(%d)\n",g_stp_dbg_cpupcr->issue_type);
	/*format common information about issue */
	len = osal_sprintf(*buf, "<main>\n\t");
	len +=
	    osal_sprintf(*buf + len, "<chipid>\n\t\tMT%x\n\t</chipid>\n\t",
			 g_stp_dbg_cpupcr->chipId);
	len += osal_sprintf(*buf + len, "<version>\n\t\t");
	len += osal_sprintf(*buf + len, "<rom>%s</rom>\n\t\t", g_stp_dbg_cpupcr->romVer);

	if (!(osal_memcmp(g_stp_dbg_cpupcr->branchVer, "ALPS", STP_PATCH_BRANCH_SZIE))) {
		len +=
		    osal_sprintf(*buf + len, "<branch>Internal Dev</branch>\n\t\t",
				 g_stp_dbg_cpupcr->branchVer);
	} else {
		len +=
		    osal_sprintf(*buf + len, "<branch>W%sMP</branch>\n\t\t",
				 g_stp_dbg_cpupcr->branchVer);
	}

	len += osal_sprintf(*buf + len, "<patch>%s</patch>\n\t\t", g_stp_dbg_cpupcr->patchVer);

	if (!g_stp_dbg_cpupcr->wifiVer[0]) {
		len += osal_sprintf(*buf + len, "<wifi>NULL</wifi>\n\t");
	} else {
		len += osal_sprintf(*buf + len, "<wifi>%s</wifi>\n\t", g_stp_dbg_cpupcr->wifiVer);
	}

	len += osal_sprintf(*buf + len, "</version>\n\t");

	/*format issue information: no ack, assert */
	len += osal_sprintf(*buf + len, "<issue>\n\t\t<classification>\n\t\t\t");

	if ((STP_FW_NOACK_ISSUE == g_stp_dbg_cpupcr->issue_type) ||
	    (STP_DBG_PROC_TEST == g_stp_dbg_cpupcr->issue_type) ||
	    (STP_FW_WARM_RST_ISSUE == g_stp_dbg_cpupcr->issue_type)) {
		len +=
		    osal_sprintf(*buf + len, "%s\n\t\t</classification>\n\t\t<rc>\n\t\t\t",
				 g_stp_dbg_cpupcr->assert_info);
		len += osal_sprintf(*buf + len, "NULL\n\t\t</rc>\n\t</issue>\n\t");
		len += osal_sprintf(*buf + len, "<hint>\n\t\t<time_align>NULL</time_align>\n\t\t");
		len += osal_sprintf(*buf + len, "<host>NULL</host>\n\t\t");
		len +=
		    osal_sprintf(*buf + len, "<client>\n\t\t\t<task>%s</task>\n\t\t\t",
				 _stp_dbg_id_to_task(g_stp_dbg_cpupcr->fwTaskId));
		len +=
		    osal_sprintf(*buf + len, "<irqx>IRQ_0x%x</irqx>\n\t\t\t",
				 g_stp_dbg_cpupcr->fwRrq);
		len += osal_sprintf(*buf + len, "<isr>0x%x</isr>\n\t\t\t", g_stp_dbg_cpupcr->fwIsr);
	} else if ((STP_FW_ASSERT_ISSUE == g_stp_dbg_cpupcr->issue_type)) {
		len +=
		    osal_sprintf(*buf + len, "%s\n\t\t</classification>\n\t\t<rc>\n\t\t\t",
				 g_stp_dbg_cpupcr->assert_info);
		len +=
		    osal_sprintf(*buf + len, "%s\n\t\t</rc>\n\t</issue>\n\t",
				 g_stp_dbg_cpupcr->assert_type);
		len += osal_sprintf(*buf + len, "<hint>\n\t\t<time_align>NULL</time_align>\n\t\t");
		len += osal_sprintf(*buf + len, "<host>NULL</host>\n\t\t");
		len +=
		    osal_sprintf(*buf + len, "<client>\n\t\t\t<task>%s</task>\n\t\t\t",
				 _stp_dbg_id_to_task(g_stp_dbg_cpupcr->fwTaskId));
		len +=
		    osal_sprintf(*buf + len, "<irqx>IRQ_0x%x</irqx>\n\t\t\t",
				 g_stp_dbg_cpupcr->fwRrq);
		len += osal_sprintf(*buf + len, "<isr>0x%x</isr>\n\t\t\t", g_stp_dbg_cpupcr->fwIsr);
	} else {
		len += osal_sprintf(*buf + len, "NULL\n\t\t</classification>\n\t\t<rc>\n\t\t\t");
		len += osal_sprintf(*buf + len, "NULL\n\t\t</rc>\n\t</issue>\n\t");
		len += osal_sprintf(*buf + len, "<hint>\n\t\t<time_align>NULL</time_align>\n\t\t");
		len += osal_sprintf(*buf + len, "<host>NULL</host>\n\t\t");
		len += osal_sprintf(*buf + len, "<client>\n\t\t\t<task>NULL</task>\n\t\t\t");
		len += osal_sprintf(*buf + len, "<irqx>NULL</irqx>\n\t\t\t");
		len += osal_sprintf(*buf + len, "<isr>NULL</isr>\n\t\t\t");
	}

	len += osal_sprintf(*buf + len, "<pctrace>");
	STP_DBG_INFO_FUNC("stp-dbg:sub len1 for debug(%d)\n", len);

	if (!g_stp_dbg_cpupcr->count) {
		len += osal_sprintf(*buf + len, "NULL");
	} else {
		for (i = 0; i < g_stp_dbg_cpupcr->count; i++) {
			len += osal_sprintf(*buf + len, "%08x,", g_stp_dbg_cpupcr->buffer[i]);
		}
	}

	STP_DBG_INFO_FUNC("stp-dbg:sub len2 for debug(%d)\n", len);
	len += osal_sprintf(*buf + len, "</pctrace>\n\t\t\t");
	len +=
	    osal_sprintf(*buf + len,
			 "<extension>NULL</extension>\n\t\t</client>\n\t</hint>\n</main>\n");
	STP_DBG_INFO_FUNC("buffer len[%d]\n", len);
	/* STP_DBG_INFO_FUNC("Format infor:\n%s\n",*buf); */
	*str_len = len;
	osal_memset(&g_stp_dbg_cpupcr->buffer[0], 0, STP_DBG_CPUPCR_NUM);
	g_stp_dbg_cpupcr->count = 0;

	return 0;

}


MTKSTP_DBG_T *stp_dbg_init(PVOID btm_half)
{

	MTKSTP_DBG_T *stp_dbg = NULL;
	STP_DBG_INFO_FUNC("stp-dbg init\n");

	stp_dbg = kzalloc(sizeof(MTKSTP_DBG_T), GFP_KERNEL);
	if (IS_ERR(stp_dbg)) {
		STP_DBG_ERR_FUNC("-ENOMEM\n");
		goto ERR_EXIT1;
	}

	stp_dbg->logsys = vmalloc(sizeof(MTKSTP_LOG_SYS_T));
	if (IS_ERR(stp_dbg->logsys)) {
		STP_DBG_ERR_FUNC("-ENOMEM stp_gdb->logsys\n");
		goto ERR_EXIT2;
	}
	memset(stp_dbg->logsys, 0, sizeof(MTKSTP_LOG_SYS_T));
	spin_lock_init(&(stp_dbg->logsys->lock));
	stp_dbg->pkt_trace_no = 0;
	stp_dbg->is_enable = 0;
	g_stp_dbg = stp_dbg;

	if (btm_half != NULL) {
		stp_dbg->btm = btm_half;
	} else {
		stp_dbg->btm = NULL;
	}

	/* bind to netlink */
	stp_dbg_nl_init();

	g_core_dump = wcn_core_dump_init(STP_CORE_DUMP_TIMEOUT);

	g_stp_dbg_cpupcr = stp_dbg_cpupcr_init();

	return stp_dbg;

 ERR_EXIT2:
	kfree(stp_dbg);
	return NULL;

 ERR_EXIT1:
	return NULL;
}

INT32 stp_dbg_deinit(MTKSTP_DBG_T *stp_dbg)
{

	STP_DBG_INFO_FUNC("stp-dbg deinit\n");

	wcn_core_dump_deinit(g_core_dump);

	stp_dbg_cpupcr_deinit(g_stp_dbg_cpupcr);

	/* unbind with netlink */
	stp_dbg_nl_deinit();

	if (stp_dbg->logsys) {
		vfree(stp_dbg->logsys);
	}

	if (stp_dbg) {
		kfree(stp_dbg);
	}

	return 0;
}
