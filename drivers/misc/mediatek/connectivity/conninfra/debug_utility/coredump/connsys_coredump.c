// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/timer.h>
#if defined(CONNINFRA_PLAT_ALPS) && CONNINFRA_PLAT_ALPS
#include <aee.h>
#endif
#include "conninfra.h"
#include "connsys_debug_utility.h"
#include "connsys_coredump.h"
#include "connsys_coredump_emi.h"
#include "connsys_coredump_hw_config.h"
#include "conndump_netlink.h"

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

#define DEBUG_MODE 1

#define CONNSYS_DUMP_INFO_SIZE		180
#define CONNSYS_ASSERT_INFO_SIZE	164
#define CONNSYS_ASSERT_TYPE_SIZE	64
#define CONNSYS_ASSERT_KEYWORD_SIZE	64
#define CONNSYS_ASSERT_REASON_SIZE	128
#define CONNSYS_AEE_INFO_SIZE		240

#define INFO_HEAD ";CONSYS FW CORE,"
#define WDT_INFO_HEAD "Watch Dog Timeout"

enum fw_coredump_status {
	FW_DUMP_STATE_NOT_START = 0,
	FW_DUMP_STATE_PUTTING = 1,
	FW_DUMP_STATE_PUT_DONE = 2,
	FW_DUMP_STATE_END,
};

enum core_dump_state {
	CORE_DUMP_INIT = 0,
	CORE_DUMP_START,
	CORE_DUMP_DOING,
	CORE_DUMP_TIMEOUT, /* Coredump timeout */
	CORE_DUMP_EMI_TIMEOUT, /* EMI dump timeout */
	CORE_DUMP_EMI, /* start for EMI dump */
	CORE_DUMP_DONE,
	CORE_DUMP_INVALID,
	CORE_DUMP_MAX,
};

enum connsys_issue_type {
	CONNSYS_ISSUE_FW_ASSERT = 0,
	CONNSYS_ISSUE_FW_EXCEPTION,
	CONNSYS_ISSUE_DRIVER_ASSERT,
	CONNSYS_ISSUE_MAX,
};

struct connsys_dump_info {
	enum connsys_issue_type issue_type;
	char dump_info[CONNSYS_DUMP_INFO_SIZE];
	char assert_info[CONNSYS_ASSERT_INFO_SIZE];
	unsigned int fw_task_id;
	unsigned int fw_isr;
	unsigned int fw_irq;
	unsigned int exp_ipc;
	unsigned int exp_eva;
	char etype[CONNSYS_ASSERT_INFO_SIZE];
	char assert_type[CONNSYS_ASSERT_TYPE_SIZE];
	char exception_log[CONNSYS_AEE_INFO_SIZE];
	int drv_type;
	char reason[CONNSYS_ASSERT_REASON_SIZE];
	char keyword[CONNSYS_ASSERT_KEYWORD_SIZE];
};

struct connsys_dump_ctx {
	int conn_type;
	phys_addr_t emi_phy_addr_base;
	void __iomem *emi_virt_addr_base;
	unsigned int emi_size;
	unsigned int mcif_emi_size;
	struct coredump_event_cb callback;
	struct timer_list dmp_timer;
	struct completion emi_dump;
	enum core_dump_state sm;
	char* page_dump_buf;
	struct dump_region* dump_regions;
	int dump_regions_num;
	unsigned int dynamic_map_cr;
	struct coredump_hw_config hw_config;
	struct connsys_dump_info info;
	unsigned int full_emi_size;
};

#define DUMP_REGION_NAME_LENGTH	10
/* TODO: Need check */
#define DUMP_REGION_DEFAULT_NUM	50

enum dump_region_type {
	DUMP_TYPE_CR = 0,
	DUMP_TYPE_ROM,
	DUMP_TYPE_ILM,
	DUMP_TYPE_DLM,
	DUMP_TYPE_SRAM,
	DUMP_TYPE_SYS1,
	DUMP_TYPE_SYS2,
	DUMP_TYPE_MAX,
};

struct dump_region {
	int dump_type;
	char name[DUMP_REGION_NAME_LENGTH];
	unsigned int base;
	unsigned int length;
};

static atomic_t g_dump_mode = ATOMIC_INIT(DUMP_MODE_DAEMON);

static const char* g_type_name[] = {
	"Wi-Fi",
	"BT",
};

/*******************************************************************************
*                             MACROS
********************************************************************************
*/

#if DEBUG_MODE
#if defined(CONFIG_FPGA_EARLY_PORTING)
/* For FPGA shorten the timer */
#define CONNSYS_COREDUMP_TIMEOUT	(1*10*1000)
#define CONNSYS_EMIDUMP_TIMEOUT		(10*1000)
#else
#define CONNSYS_COREDUMP_TIMEOUT	(1*10*1000)
#define CONNSYS_EMIDUMP_TIMEOUT		(60*1000)
#endif
#else
#define CONNSYS_COREDUMP_TIMEOUT	(10*1000)
#define CONNSYS_EMIDUMP_TIMEOUT		(60*1000)
#endif

#define CONNSYS_DUMP_BUFF_SIZE (32*1024*sizeof(char))

#define EMI_READ32(addr) (readl(addr))
#define EMI_READ8(addr) (readb(addr))
#define DUMP_LOG_BYTES_PER_LINE (128)
#define IS_VISIBLE_CHAR(c) ((c) >= 32 && (c) <= 126)

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static void conndump_set_dump_state(struct connsys_dump_ctx* ctx, enum core_dump_state state);
static enum core_dump_state conndump_get_dump_state(struct connsys_dump_ctx* ctx);
static void conndump_timeout_handler(unsigned long data);
static inline int conndump_get_dmp_info(struct connsys_dump_ctx* ctx, unsigned int offset, bool log);
/* Dynamic remap relative function */
static unsigned int conndump_setup_dynamic_remap(struct connsys_dump_ctx* ctx, unsigned int base, unsigned int length);
static void __iomem* conndump_remap(struct connsys_dump_ctx* ctx, unsigned int base, unsigned int length);
static void conndump_unmap(struct connsys_dump_ctx* ctx, void __iomem* vir_addr);
/* To dump different partition */
static void conndump_dump_log(char* buf, int size);
static int conndump_dump_print_buff(struct connsys_dump_ctx* ctx);
static int conndump_dump_dump_buff(struct connsys_dump_ctx* ctx);
static int conndump_dump_cr_regions(struct connsys_dump_ctx* ctx);
static int conndump_dump_mem_regions(struct connsys_dump_ctx* ctx);
static int conndump_dump_emi(struct connsys_dump_ctx* ctx);
static int conndump_send_fake_coredump(struct connsys_dump_ctx* ctx);
/* Utility */
static bool conndump_check_cr_readable(struct connsys_dump_ctx* ctx);
static int conndump_info_format(
	struct connsys_dump_ctx* ctx,
	char* buf, unsigned int max_len,
	bool full_dump);
static void conndump_exception_show(struct connsys_dump_ctx* ctx, bool full_dump);
/* memory allocate/free */
static void* conndump_malloc(unsigned int size);
static void conndump_free(const void* dst);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/* Platform relative function */
/* ------------------------------------------------------------------------------*/
struct coredump_hw_config* __weak get_coredump_platform_config(int conn_type)
{
	pr_err("[%s] Miss platform ops!\n", __func__);
	return NULL;
}

unsigned int __weak get_coredump_platform_chipid(void)
{
	pr_err("[%s] Miss platform ops!\n", __func__);
	return 0;
}

char* __weak get_task_string(int conn_type, int task_id)
{
	pr_err("[%s] Miss platform ops!\n", __func__);
	return "ERROR";
}

char* __weak get_sys_name(int conn_type)
{
	pr_err("[%s] Miss platform ops!\n", __func__);
	return "ERROR";
}

bool __weak is_host_view_cr(unsigned int addr, unsigned int* host_view)
{
	pr_err("[%s] Miss platform ops!\n", __func__);
	return false;
}

/*----------------------------------------------------------------------------*/


static unsigned long timeval_to_ms(struct timeval *begin, struct timeval *end)
{
	unsigned long time_diff;

	time_diff = (end->tv_sec - begin->tv_sec) * 1000;
	time_diff += (end->tv_usec - begin->tv_usec) / 1000;

	return time_diff;
}

static void* conndump_malloc(unsigned int size)
{
	void* p = NULL;

	if (size > (PAGE_SIZE << 1))
		p = vmalloc(size);
	else
		p = kmalloc(size, GFP_KERNEL);

	/* If there is fragment, kmalloc may not get memory when size > one page.
	 * For this case, use vmalloc instead.
	 */
	if (p == NULL && size > PAGE_SIZE)
		p = vmalloc(size);
	return p;
}

static void conndump_free(const void* dst)
{
	kvfree(dst);
}


static bool conndump_check_cr_readable(struct connsys_dump_ctx* ctx)
{
	int cb_ret = 0;

	/* Check reg readable */
	if (ctx->callback.reg_readable) {
		cb_ret = ctx->callback.reg_readable();
		if (cb_ret == 1) {
			return true;
		}
		pr_err("[%s] reg_readable callback ret = %d\n",
			g_type_name[ctx->conn_type], cb_ret);
		return false;
	}

	pr_err("[%s] reg_readable callback is not implement! Return false\n", g_type_name[ctx->conn_type]);
	return false;
}

static void conndump_set_dump_state(
	struct connsys_dump_ctx* ctx, enum core_dump_state state)
{
	if (ctx)
		ctx->sm = state;
}

static enum core_dump_state conndump_get_dump_state(struct connsys_dump_ctx* ctx)
{
	if (ctx)
		return ctx->sm;
	pr_err("%s ctx is null\n", __func__);
	return CORE_DUMP_INVALID;
}

static void conndump_timeout_handler(unsigned long data)
{
	struct connsys_dump_ctx* ctx = (struct connsys_dump_ctx*)data;

	pr_info("[%s] coredump timeout\n", ctx->conn_type);
	conndump_set_dump_state(ctx, CORE_DUMP_TIMEOUT);
}

static inline void conndump_get_dmp_char(struct connsys_dump_ctx* ctx, unsigned int offset, unsigned int byte_num, char* dest)
{
	int i;
	char ret;

	if (!dest)
		return;

	for (i = 0; i < byte_num; i++) {
		ret = EMI_READ8(ctx->emi_virt_addr_base + offset + i);
		dest[i] = ret;
	}
}

static inline int conndump_get_dmp_info(struct connsys_dump_ctx* ctx, unsigned int offset, bool log)
{
	int ret;

	ret = EMI_READ32(ctx->emi_virt_addr_base + offset);
	if (log)
		pr_info("EMI[0x%x] = 0x%08x\n", offset, ret);

	return ret;
}

static void conndump_dump_log(char* buf, int size)
{
	int i = 0;
	char line[DUMP_LOG_BYTES_PER_LINE + 1];

	while (size--) {
		if (IS_VISIBLE_CHAR(*buf))
			line[i] = *buf;
		else
			line[i] = '.';
		i++;
		buf++;

		if (i >= DUMP_LOG_BYTES_PER_LINE || !size) {
			line[i] = 0;
			pr_info("page_trace: %s\n", line);
			i = 0;
		}
	}
}

static int conndump_info_format(
	struct connsys_dump_ctx* ctx,
	char* buf,
	unsigned int max_len,
	bool full_dump)
{
#define FORMAT_STRING(buf, len, max_len, sec_len, fmt, arg...) \
do { \
	sec_len = snprintf(buf + len, max_len, fmt, ##arg); \
	max_len -= sec_len; \
	len += sec_len; \
	if (max_len <= 0) \
		goto format_finish; \
} while (0)

	int len = 0;
	int ret;
	int idx;
	int sec_len;
	char* drv_name[CONNDRV_TYPE_MAX] = {
		"DRV_BT",
		"DRV_FM",
		"DRV_GPS",
		"DRV_WIFI",
		"DRV_CONNINFRA",
	};
	/* Align with connac1.x and previous version */
	char* task_drv_name[CONNDRV_TYPE_MAX] = {
		"Task_DrvBT",
		"Task_DrvFM",
		"Task_DrvGPS",
		"Task_DrvWifi",
		"Task_DrvConninfra",
	};
	char* task_drv_name2[CONN_DEBUG_TYPE_END] = {
		"Task_DrvWifi",
		"Task_DrvBT",
	};

	if (!buf) {
		pr_err("Invalid input, buf = %p", buf);
		return 0;
	}

	/* Init buffer */
	memset(buf, '\0', max_len);
	FORMAT_STRING(buf, len, max_len, sec_len, "<main>\n");
	FORMAT_STRING(buf, len, max_len, sec_len, "\t<chipid>MT%x</chipid>\n", get_coredump_platform_chipid());

	/* <issue> section */
	FORMAT_STRING(buf, len, max_len, sec_len,
		"\t<issue>\n\t\t<classification>\n\t\t\t%s\n\t\t</classification>\n",
		ctx->info.assert_info);
	if (ctx->info.issue_type == CONNSYS_ISSUE_FW_EXCEPTION) {
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t<rc>\n\t\t\tFW Exception: %s\n\t\t</rc>\n", ctx->info.etype);
	} else if (ctx->info.issue_type == CONNSYS_ISSUE_FW_ASSERT) {
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t<rc>\n\t\t\t%s\n\t\t</rc>\n", ctx->info.assert_type);
	} else if (ctx->info.issue_type == CONNSYS_ISSUE_DRIVER_ASSERT) {
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t<rc>\n\t\t\t%s trigger assert\n\t\t</rc>\n", drv_name[ctx->info.drv_type]);
	} else {
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t<rc>\n\t\t\tUnknown\n\t\t</rc>\n");
	}
	/* <hint><client> section @{*/
	FORMAT_STRING(buf, len, max_len, sec_len,
		"\t</issue>\n\t<hint>\n\t\t<client>\n");

	FORMAT_STRING(buf, len, max_len, sec_len,
		"\t\t\t<subsys>%s</subsys>\n", ctx->hw_config.name);
	if (ctx->info.issue_type == CONNSYS_ISSUE_DRIVER_ASSERT) {
		/* Driver trigger assert */
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t\t<task>%s</task>\n",
			task_drv_name[ctx->info.drv_type]);
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t\t<irqx>NULL</irqx>\n");
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t\t<isr>NULL</isr>\n");
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t\t<drv_type>%s</drv_type>\n",
			drv_name[ctx->info.drv_type]);
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t\t<reason>%s</reason>\n",
			ctx->info.reason);
	} else {
		/* FW assert */
		if (full_dump) {
			FORMAT_STRING(buf, len, max_len, sec_len,
				"\t\t\t<task>%s</task>\n",
				get_task_string(ctx->conn_type, ctx->info.fw_task_id));
			FORMAT_STRING(buf, len, max_len, sec_len,
				"\t\t\t<irqx>IRQ_0x%x_%s</irqx>\n",
				ctx->info.fw_irq,
				get_sys_name(ctx->conn_type));
			FORMAT_STRING(buf, len, max_len, sec_len,
				"\t\t\t<isr>0x%x</isr>\n",
				ctx->info.fw_isr);
		} else {
			FORMAT_STRING(buf, len, max_len, sec_len,
				"\t\t\t<task>%s</task>\n",
				task_drv_name2[ctx->conn_type]);
			FORMAT_STRING(buf, len, max_len, sec_len,
				"\t\t\t<irqx>NULL</irqx>\n");
			FORMAT_STRING(buf, len, max_len, sec_len,
				"\t\t\t<isr>NULL</isr>\n");
		}
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t\t<drv_type>%s</drv_type>\n",
			"NULL");
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t\t<reason>%s</reason>\n",
			"NULL");
	}
	if (strlen(ctx->info.keyword) != 0) {
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t\t<keyword>%s</keyword>\n",
			ctx->info.keyword);
	} else {
		FORMAT_STRING(buf, len, max_len, sec_len,
			"\t\t\t<keyword>NULL</keyword>\n");

	}
	FORMAT_STRING(buf, len, max_len, sec_len,
		"\t\t</client>\n\t</hint>\n");
	/*<hint><client> section @}*/
	/* <map> section start @{ */
	FORMAT_STRING(buf, len, max_len, sec_len, "\t<map>\n");
	for (idx = 0; idx < ctx->dump_regions_num; idx++) {
		/* Non-empty name means memory regions  */
		if (ctx->dump_regions[idx].name[0] != 0) {
			FORMAT_STRING(buf, len, max_len, sec_len,
				"\t\t<%s>\n\t\t\t<offset>0x%x</offset>\n\t\t\t<size>0x%x</size>\n\t\t</%s>\n",
				ctx->dump_regions[idx].name,
				ctx->dump_regions[idx].base,
				ctx->dump_regions[idx].length,
				ctx->dump_regions[idx].name);
		}
	}
	FORMAT_STRING(buf, len, max_len, sec_len, "\t</map>\n");
	/* <map> section end @} */
	FORMAT_STRING(buf, len, max_len, sec_len, "</main>\n");

format_finish:
	pr_info("== Issue info ==\n", buf);
	pr_info("%s\n", buf);
	pr_info("===== END =====\n");
	ret = conndump_netlink_send_to_native(ctx->conn_type, "INFO", buf, len);
	if (ret < 0)
		pr_err("Send issue info to native fail, ret=%d\n", ret);
	return len;
}

static void conndump_info_analysis(
	struct connsys_dump_ctx* ctx,
	unsigned int buf_len)
{
	const char* parser_sub_string[] = {
		"<ASSERT> ",
		"id=",
		"isr=",
		"irq=",
		"rc=",
	};
	const char* exception_sub_string[] = {
		"<EXCEPTION> ",
		"ipc=",
		"eva=",
		"etype:",
	};
	char* pStr = ctx->page_dump_buf;
	char* pDtr = NULL;
	char* pTemp = NULL;
	char* pTemp2 = NULL;
	unsigned int len;
	int remain_array_len = 0, sec_len = 0, idx = 0;
	char tempBuf[CONNSYS_ASSERT_TYPE_SIZE] = { 0 };
	int ret, res;
	char* type_str;

	/* Check <ASSERT> */
	memset(&ctx->info.assert_info[0], '\0', CONNSYS_ASSERT_INFO_SIZE);
	type_str = (char*)parser_sub_string[0];
	pDtr = strstr(pStr, type_str);
	if (pDtr != NULL) {
		if (ctx->info.issue_type != CONNSYS_ISSUE_DRIVER_ASSERT)
			ctx->info.issue_type = CONNSYS_ISSUE_FW_ASSERT;
		pDtr += strlen(type_str);
		pTemp = strchr(pDtr, ' ');

		if (pTemp != NULL) {
			idx = 0;
			remain_array_len = CONNSYS_ASSERT_INFO_SIZE -1;
			sec_len = strlen("assert@");
			memcpy(&ctx->info.assert_info[0], "assert@", sec_len);
			idx += sec_len;
			remain_array_len -= sec_len;

			len = pTemp - pDtr;
			sec_len = (len < remain_array_len ? len : remain_array_len);
			if (sec_len > remain_array_len)
				goto check_task_id;
			memcpy(&ctx->info.assert_info[idx], pDtr, sec_len);
			remain_array_len -= sec_len;
			idx += sec_len;

			sec_len = strlen("_");
			if (sec_len > remain_array_len)
				goto check_task_id;
			ctx->info.assert_info[idx] = '_';
			remain_array_len -= sec_len;
			idx += sec_len;

			pTemp = strchr(pDtr, '#');
			if (pTemp != NULL) {
				pTemp += 1;
				pTemp2 = strchr(pTemp, ' ');
				if (pTemp2 == NULL) {
					pr_err("parser ' ' is not find\n");
					pTemp2 = pTemp + 1;
				}
				pr_info("(pTemp2 - pTemp)=%d\n", (pTemp2 - pTemp));
				if ((remain_array_len) > (pTemp2 - pTemp)) {
					pr_info("Copy %d\n", pTemp2 - pTemp);
					memcpy(
						&ctx->info.assert_info[idx],
						pTemp,
						pTemp2 - pTemp);
				} else {
					pr_info("copy %d\n", (remain_array_len - 1));
					memcpy(
						&ctx->info.assert_info[idx],
						pTemp,
						remain_array_len);
				}
			}
			pr_info("assert info:%s\n", ctx->info.assert_info);
		}
	} else {
		/* FW exception */
		type_str = (char*)exception_sub_string[0];
		pDtr = strstr(pStr, type_str);
		if (pDtr == NULL)
			goto check_task_id;

		if (ctx->info.issue_type != CONNSYS_ISSUE_DRIVER_ASSERT)
			ctx->info.issue_type = CONNSYS_ISSUE_FW_EXCEPTION;
		idx = 0;
		remain_array_len = CONNSYS_ASSERT_INFO_SIZE;
		sec_len = snprintf(&ctx->info.assert_info[idx], remain_array_len,
			"%s", "Exception:");
		remain_array_len -= sec_len;
		idx += sec_len;

		/* Check ipc */
		type_str = (char*)exception_sub_string[1];
		pDtr = strstr(pDtr, type_str);
		if (pDtr == NULL) {
			pr_err("Exception substring (%s) not found\n", type_str);
			goto check_task_id;
		}
		pDtr += strlen(type_str);
		pTemp = strchr(pDtr, ',');
		if (pTemp != NULL) {
			len = pTemp - pDtr;
			len = (len >= CONNSYS_ASSERT_TYPE_SIZE) ? CONNSYS_ASSERT_TYPE_SIZE - 1 : len;
			memcpy(&tempBuf[0], pDtr, len);
			tempBuf[len] = '\0';
			ret = kstrtouint(tempBuf, 16, &res);
			if (ret) {
				pr_err("Convert to uint fail, ret=%d, buf=%s\n",
					ret, tempBuf);
			} else {
				ctx->info.exp_ipc = res;
				pr_info("exp_ipc=0x%x\n", ctx->info.exp_ipc);
			}
			if (remain_array_len > 0) {
				sec_len = snprintf(&ctx->info.assert_info[idx], remain_array_len,
						" ipc=0x%x", ctx->info.exp_ipc);
				remain_array_len -= sec_len;
				idx += sec_len;
			}
		}

		/* Check eva */
		type_str = (char*)exception_sub_string[2];
		pDtr = strstr(pDtr, type_str);
		if (pDtr == NULL) {
			pr_err("substring (%s) not found\n", type_str);
			goto check_task_id;
		}
		pDtr += strlen(type_str);
		pTemp = strchr(pDtr, ',');
		if (pTemp != NULL) {
			len = pTemp - pDtr;
			len = (len >= CONNSYS_ASSERT_TYPE_SIZE) ? CONNSYS_ASSERT_TYPE_SIZE - 1 : len;
			memcpy(&tempBuf[0], pDtr, len);
			tempBuf[len] = '\0';
			ret = kstrtouint(tempBuf, 16, &res);
			if (ret) {
				pr_err("Convert to uint fail, ret=%d, buf=%s\n",
					ret, tempBuf);
			} else {
				ctx->info.exp_eva = res;
				pr_info("eva addr=0x%x\n", ctx->info.exp_eva);
			}
			if (remain_array_len > 0) {
				sec_len = snprintf(
					&ctx->info.assert_info[idx], remain_array_len,
					" eva=0x%x", ctx->info.exp_eva);
				remain_array_len -= sec_len;
				idx += sec_len;
			}
		}

		/* Check etype */
		type_str = (char*)exception_sub_string[3];
		pDtr = strstr(pDtr, type_str);
		if (pDtr == NULL) {
			pr_err("Substring(%s) not found\n", type_str);
			goto check_task_id;
		}
		pDtr += strlen(type_str);
		pTemp = strchr(pDtr, ',');
		if (pTemp != NULL) {
			len = pTemp - pDtr;
			len = (len >= CONNSYS_ASSERT_TYPE_SIZE) ? CONNSYS_ASSERT_TYPE_SIZE - 1 : len;
			memcpy(ctx->info.etype, pDtr, len);
			ctx->info.etype[len] = '\0';
			pr_info("etype=%s\n", ctx->info.etype);
			if (remain_array_len > 0) {
				sec_len = snprintf(
					&ctx->info.assert_info[idx], remain_array_len,
					" etype=%s", ctx->info.etype);
				remain_array_len -= sec_len;
				idx += sec_len;
			}
		}
	}

check_task_id:
	/* Check id */
	ctx->info.fw_task_id = 0;
	type_str = (char*)parser_sub_string[1];
	pDtr = strstr(pStr, type_str);
	if (pDtr == NULL) {
		pr_err("parser str is NULL,substring(%s)\n", type_str);

	} else {
		pDtr += strlen(type_str);
		pTemp = strchr(pDtr, ' ');

		if (pTemp == NULL) {
			pr_err("delimiter( ) is not found,substring(%s)\n",
				type_str);
		} else {
			len = pTemp - pDtr;
			len = (len >= CONNSYS_ASSERT_TYPE_SIZE) ? CONNSYS_ASSERT_TYPE_SIZE - 1 : len;
			memcpy(&tempBuf[0], pDtr, len);
			tempBuf[len] = '\0';
			ret = kstrtouint(tempBuf, 16, &res);
			if (ret) {
				pr_err("Convert to uint fail, ret=%d\n, buf=%s\n",
					ret, tempBuf);
			} else {
				ctx->info.fw_task_id = res;
				pr_info("fw task id: %x\n", ctx->info.fw_task_id);
			}
		}
	}

	/* Check ISR */
	ctx->info.fw_isr = 0;
	type_str = (char*)parser_sub_string[2];
	pDtr = strstr(pStr, type_str);
	if (pDtr == NULL) {
		pr_err("parser str is NULL,substring(%s)\n", type_str);
	} else {
		pDtr += strlen(type_str);
		pTemp = strchr(pDtr, ',');

		if (pTemp == NULL) {
			pr_err("delimiter(,) is not found,substring(%s)\n", type_str);
		} else {
			len = pTemp - pDtr;
			len = (len >= CONNSYS_ASSERT_TYPE_SIZE) ? CONNSYS_ASSERT_TYPE_SIZE - 1 : len;
			memcpy(&tempBuf[0], pDtr, len);
			tempBuf[len] = '\0';

			ret = kstrtouint(tempBuf, 16, &res);
			if (ret) {
				pr_err("Get fw isr id fail, ret=%d, buf=%s\n", ret, tempBuf);
			} else {
				ctx->info.fw_isr = res;
				pr_info("fw isr str:%x\n", ctx->info.fw_isr);
			}
		}
	}

	/* Check IRQ */
	ctx->info.fw_irq = 0;
	type_str = (char*)parser_sub_string[3];
	pDtr = strstr(pStr, type_str);
	if (pDtr == NULL) {
		pr_err("parser str is NULL,substring(%s)\n", type_str);
	} else {
		pDtr += strlen(type_str);
		pTemp = strchr(pDtr, ',');
		if (pTemp == NULL) {
			pr_err("delimiter(,) is not found,substring(%s)\n", type_str);
		} else {
			len = pTemp - pDtr;
			len = (len >= CONNSYS_ASSERT_TYPE_SIZE) ? CONNSYS_ASSERT_TYPE_SIZE - 1 : len;
			memcpy(&tempBuf[0], pDtr, len);
			tempBuf[len] = '\0';
			ret = kstrtouint(tempBuf, 16, &res);
			if (ret) {
				pr_err("get fw irq id fail ret=%d, buf=%s\n", ret, tempBuf);
			} else {
				ctx->info.fw_irq = res;
				pr_info("fw irq value:%x\n", ctx->info.fw_irq);
			}
		}
	}

	/* Check assert type */
	memset(&ctx->info.assert_type[0], '\0', CONNSYS_ASSERT_TYPE_SIZE);
	type_str = (char*)parser_sub_string[4];
	pDtr = strstr(pStr, type_str);
	if (pDtr == NULL) {
		pr_err("parser str is NULL,substring(%s)\n", type_str);
	} else {
		pDtr += strlen(type_str);
		pTemp = strchr(pDtr, ',');

		if (pTemp == NULL) {
			pr_err("delimiter(,) is not found,substring(%s)\n", type_str);
		} else {
			len = pTemp - pDtr;
			len = (len >= CONNSYS_ASSERT_TYPE_SIZE) ? CONNSYS_ASSERT_TYPE_SIZE - 1 : len;
			memcpy(&tempBuf[0], pDtr, len);
			tempBuf[len] = '\0';

			if (memcmp(tempBuf, "*", strlen("*")) == 0)
				memcpy(
					&ctx->info.assert_type[0],
					"general assert",
					strlen("general assert"));
			if (memcmp(tempBuf, WDT_INFO_HEAD, strlen(WDT_INFO_HEAD)) == 0)
				memcpy(
					&ctx->info.assert_type[0], "wdt", strlen("wdt"));
			if (memcmp(tempBuf, "RB_FULL", strlen("RB_FULL")) == 0) {
				memcpy(&ctx->info.assert_type[0], tempBuf, len);
				pDtr = strstr(&ctx->info.assert_type[0], "RB_FULL(");
				if (pDtr == NULL) {
					pr_err("parser str is NULL,substring(RB_FULL()\n");
				} else {
					pDtr += strlen("RB_FULL(");
					pTemp = strchr(pDtr, ')');
					len = pTemp - pDtr;
					len = (len >= CONNSYS_ASSERT_TYPE_SIZE) ? CONNSYS_ASSERT_TYPE_SIZE - 1 : len;
					memcpy(&tempBuf[0], pDtr, len);
					tempBuf[len] = '\0';
					ret = kstrtouint(tempBuf, 16, &res);
					if (ret) {
						pr_err("get fw task id fail(%d)\n", ret);
					} else {
						snprintf(
							ctx->info.keyword,
							CONNSYS_ASSERT_KEYWORD_SIZE,
							"RB_FULL_%d_%s", res,
							ctx->hw_config.name);
					}
				}
			}
		}
		pr_info("fw asert type:%s\n", ctx->info.assert_type);
	}

	/* Store first several characters */
	len = strlen(ctx->page_dump_buf);
	len = (len >= CONNSYS_DUMP_INFO_SIZE) ? CONNSYS_DUMP_INFO_SIZE - 1 : len;
	strncpy(ctx->info.dump_info, ctx->page_dump_buf, CONNSYS_DUMP_INFO_SIZE);
	ctx->info.dump_info[len] = '\0';
}

/*****************************************************************************
 * FUNCTION
 *  conndump_dump_print_buff
 * DESCRIPTION
 *
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
static int conndump_dump_print_buff(struct connsys_dump_ctx* ctx)
{
	int ret = 0;
	unsigned int buff_start, buff_len;
	int section_len;

	buff_start = CONNSYS_DUMP_PRINT_BUFF_OFFSET;
	buff_len = conndump_get_dmp_info(ctx, CONNSYS_DUMP_CTRL_BLOCK_OFFSET + EXP_CTRL_PRINT_BUFF_IDX, true);

	if (buff_len > CONNSYS_DUMP_PRINT_BUFF_SIZE) {
		pr_err("Get print buff idx = %d, but the length is %d\n", buff_len, CONNSYS_DUMP_PRINT_BUFF_SIZE);
		buff_len = CONNSYS_DUMP_PRINT_BUFF_SIZE;
	}

	pr_info("-- paged trace ascii output start --\n");
	while (buff_len > 0) {
		memset(ctx->page_dump_buf, '\0', CONNSYS_DUMP_BUFF_SIZE);
		section_len = (buff_len > (CONNSYS_DUMP_BUFF_SIZE - 1)) ? (CONNSYS_DUMP_BUFF_SIZE - 1) : buff_len;
		memcpy_fromio(ctx->page_dump_buf, ctx->emi_virt_addr_base + buff_start, section_len);

		pr_info("-- paged trace ascii output --\n");
		conndump_dump_log(ctx->page_dump_buf, section_len);

		buff_len -= section_len;
		buff_start += section_len;
	}
	pr_info("-- paged trace ascii output end --\n");
	return ret;
}

/*****************************************************************************
 * FUNCTION
 *  conndump_dump_dump_buff
 * DESCRIPTION
 *
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
static int conndump_dump_dump_buff(struct connsys_dump_ctx* ctx)
{
	int ret = 0;
	unsigned int buff_start, buff_len;
	int section_len;
	bool first = true;

	buff_start = CONNSYS_DUMP_DUMP_BUFF_OFFSET;
	buff_len = conndump_get_dmp_info(ctx, CONNSYS_DUMP_CTRL_BLOCK_OFFSET + EXP_CTRL_DUMP_BUFF_IDX, true);

	if (buff_len > CONNSYS_DUMP_DUMP_BUFF_SIZE) {
		pr_err("Get dump buff idx = %d, but the size is %d\n", buff_len, CONNSYS_DUMP_DUMP_BUFF_SIZE);
		buff_len = CONNSYS_DUMP_DUMP_BUFF_SIZE;
	}

	if (buff_len == 0) {
		ret = conndump_send_fake_coredump(ctx);
		return ret;
	}

	while (buff_len > 0) {
		memset(ctx->page_dump_buf, '\0', CONNSYS_DUMP_BUFF_SIZE);
		section_len = (buff_len > (CONNSYS_DUMP_BUFF_SIZE - 1)) ? (CONNSYS_DUMP_BUFF_SIZE - 1): buff_len;
		memcpy_fromio(ctx->page_dump_buf, ctx->emi_virt_addr_base + buff_start, section_len);

		/* pack it */
		ret = conndump_netlink_send_to_native(ctx->conn_type, "[M]", ctx->page_dump_buf, section_len);
		/* For 1st package, analysis it */
		if (first) {
			conndump_info_analysis(ctx, section_len);
			first = false;
		}

		buff_len -= section_len;
		buff_start += section_len;
	}
	return ret;
}

/*****************************************************************************
 * FUNCTION
 *  conndump_setup_dynamic_remap
 * DESCRIPTION
 *  Setup dynamic remap region
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
static unsigned int conndump_setup_dynamic_remap(struct connsys_dump_ctx* ctx, unsigned int base, unsigned int length)
{

#define DYNAMIC_MAP_MAX_SIZE	0x300000
	unsigned int map_len = (length > DYNAMIC_MAP_MAX_SIZE? DYNAMIC_MAP_MAX_SIZE : length);
	void __iomem* vir_addr = 0;

	if (is_host_view_cr(base, NULL)) {
		pr_info("Host view CR: 0x%x, skip dynamic remap\n", base);
		return length;
	}

	/* Expand to request size */
	vir_addr = ioremap_nocache(ctx->hw_config.seg1_cr, 4);
	if (vir_addr) {
		iowrite32(ctx->hw_config.seg1_phy_addr + map_len, vir_addr);
		iounmap(vir_addr);
	} else {
		return 0;
	}
	/* Setup map base */
	vir_addr = ioremap_nocache(ctx->hw_config.seg1_start_addr, 4);
	if (vir_addr) {
		iowrite32(base, vir_addr);
		iounmap(vir_addr);
	} else {
		return 0;
	}
	return map_len;
}

/*****************************************************************************
 * FUNCTION
 *  conndump_remap
 * DESCRIPTION
 *  Map dynamic remap CR region to virtual address
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
static void __iomem* conndump_remap(struct connsys_dump_ctx* ctx, unsigned int base, unsigned int length)
{
	void __iomem* vir_addr = 0;
	unsigned int host_cr;

	if (is_host_view_cr(base, &host_cr)) {
		pr_info("Map 0x%x to 0x%x\n", base, host_cr);
		vir_addr = ioremap_nocache(host_cr, length);
	} else {
		vir_addr = ioremap_nocache(ctx->hw_config.seg1_phy_addr, length);
	}
	return vir_addr;
}

/*****************************************************************************
 * FUNCTION
 *  conndump_unmap
 * DESCRIPTION
 *  Unmap virtual address
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
static void conndump_unmap(struct connsys_dump_ctx* ctx, void __iomem* vir_addr)
{
	iounmap(vir_addr);
}


/*****************************************************************************
 * FUNCTION
 *  conndump_dump_cr_regions
 * DESCRIPTION
 *
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
static int conndump_dump_cr_regions(struct connsys_dump_ctx* ctx)
{
#define CR_PER_LINE	11
	int ret = 0;
	int idx;
	unsigned int map_length;
	void __iomem *map_base;
	unsigned int buff_size = CONNSYS_DUMP_BUFF_SIZE;
	/* format is [addr,valu] */
	char per_cr[CR_PER_LINE];
	int i;
	int addr, value;
	unsigned int buff_idx = 0;

	if (!conndump_check_cr_readable(ctx)) {
		pr_err("CR not readable, skip cr dump\n");
		return -1;
	}

	for (idx = 0; idx < ctx->dump_regions_num; idx++) {
		/* empty name means cr regions  */
		if (ctx->dump_regions[idx].name[0] == 0 &&
		    (ctx->dump_regions[idx].base > 0) &&
		    (ctx->dump_regions[idx].base & 0x3) == 0 &&
		    (ctx->dump_regions[idx].length & 0x3) == 0) {
			pr_info("[%s][Region %d] base=0x%x size=0x%x\n", __func__, idx, ctx->dump_regions[idx].base, ctx->dump_regions[idx].length);
			map_length = conndump_setup_dynamic_remap(
					ctx, ctx->dump_regions[idx].base, ctx->dump_regions[idx].length);
			/* For CR region, we assume region size should < dynamic remap region. */
			if (map_length != ctx->dump_regions[idx].length) {
				pr_err("Expect_size=0x%x map_length=0x%x\n", ctx->dump_regions[idx].length, map_length);
			} else {
				map_base = conndump_remap(ctx, ctx->dump_regions[idx].base, map_length);
				if (!map_base) {
					pr_err("[%s][Region %d] remap fail, break\n", __func__, idx);
					break;
				}
				for (i = 0, addr = ctx->dump_regions[idx].base; i < map_length; i+=4, addr+=4) {
					value = (*((volatile unsigned int *)(map_base + i)));
					per_cr[0] = '[';
					memcpy(&per_cr[1], &addr, 4);
					per_cr[5] = ',';
					memcpy(&per_cr[6], &value, 4);
					per_cr[10] = ']';
					if (buff_size < CR_PER_LINE) {
						pr_info("[%s] Dump buffer full (%d-th cr), flush to native space.\n", __func__, i);
						/* pack it! */
						conndump_netlink_send_to_native(ctx->conn_type, "[M]", ctx->page_dump_buf, buff_idx);

						/* start from buffer start */
						buff_size = CONNSYS_DUMP_BUFF_SIZE;
						buff_idx = 0;
						memset(ctx->page_dump_buf, 0, CONNSYS_DUMP_BUFF_SIZE);
					}
					memcpy(&ctx->page_dump_buf[buff_idx], per_cr, CR_PER_LINE);
					buff_size -= CR_PER_LINE;
					buff_idx += CR_PER_LINE;
				}
				conndump_unmap(ctx, map_base);
			}
		}
	}

	/* pack remaining item */
	if (buff_idx) {
		pr_info("[%s] send remain %d bytes\n", __func__, buff_idx);
		conndump_netlink_send_to_native(ctx->conn_type, "[M]", ctx->page_dump_buf, buff_idx);
	}

	return ret;
}

/*****************************************************************************
 * FUNCTION
 *  conndump_nfra_get_phy_addr(&emi_addr, &emi_size);
 * DESCRIPTION
 *
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
static int conndump_dump_mem_regions(struct connsys_dump_ctx* ctx)
{
#define MEM_REGION_TAG_LENGTH	32
#define MEM_REGION_MAX_COPY_LENGTH	(CONNSYS_DUMP_BUFF_SIZE - MEM_REGION_TAG_LENGTH)
	int ret = 0;
	int idx;
	unsigned int map_length;
	void __iomem *map_base;
	unsigned int* dump_buff = NULL;

	pr_info("[%s] dump_regions_num=%d\n", __func__, ctx->dump_regions_num);
	/* Check reg readable */
	if (!conndump_check_cr_readable(ctx)) {
		pr_err("CR not readable, skip memory region dump\n");
		return -1;
	}
	for (idx = 0; idx < ctx->dump_regions_num; idx++) {
		/* Non-empty name means memory regions  */
		if (ctx->dump_regions[idx].name[0] != 0 &&
		    (ctx->dump_regions[idx].length > 0) &&
		    (ctx->dump_regions[idx].length & 0x3) == 0 &&
		    (ctx->dump_regions[idx].base & 0x3) == 0) {
			pr_info("[%s][Region %d][%s] base=0x%x size=0x%x\n",
				__func__, idx, ctx->dump_regions[idx].name,
				ctx->dump_regions[idx].base, ctx->dump_regions[idx].length);
			/* variable init */
			ret = 0;
			dump_buff = (unsigned int*)conndump_malloc(ctx->dump_regions[idx].length);
			if (!dump_buff) {
				pr_err("Allocate buffer for %s fail.\n",
					ctx->dump_regions[idx].name);
				goto next_mem_region;
			}
			/* Change dynamic window */
			map_length = conndump_setup_dynamic_remap(
					ctx, ctx->dump_regions[idx].base, ctx->dump_regions[idx].length);
			if (map_length != ctx->dump_regions[idx].length) {
				pr_err("Setup dynamic remap for %s fail. Expect 0x%x but 0x%x",
					ctx->dump_regions[idx].name,
					ctx->dump_regions[idx].length,
					map_length);
				goto next_mem_region;
			}
			map_base = conndump_remap(ctx, ctx->dump_regions[idx].base, map_length);
			if (!map_base) {
				pr_err("Remap %s fail.\n", ctx->dump_regions[idx].name);
				goto next_mem_region;
			}
			memcpy_fromio(dump_buff, map_base, map_length);
			conndump_unmap(ctx, map_base);
			ret = conndump_netlink_send_to_native(
				ctx->conn_type,
				ctx->dump_regions[idx].name,
				(char*)dump_buff,
				ctx->dump_regions[idx].length);
			if (ret != ctx->dump_regions[idx].length) {
				pr_err("[%s][%s] Send fail, length = 0x%x but only 0x%x send\n",
					__func__, ctx->dump_regions[idx].name,
					ctx->dump_regions[idx].length, ret);
			}
		}
next_mem_region:
		if (dump_buff) {
			conndump_free((void*)dump_buff);
			dump_buff = NULL;
		}
	}
	return ret;
}

/*****************************************************************************
 * FUNCTION
 *  conndump_dump_emi
 * DESCRIPTION
 *
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
static int conndump_dump_emi(struct connsys_dump_ctx* ctx)
{
#define EMI_COMMAND_LENGTH	64
	int ret;
	unsigned long comp_ret;
	// format: emi_size=aaaaaaaa, mcif_emi_size=bbbbbbbb
	char emi_dump_command[EMI_COMMAND_LENGTH];

	snprintf(emi_dump_command, EMI_COMMAND_LENGTH, "emi_size=%d,mcif_emi_size=%d", ctx->full_emi_size, ctx->mcif_emi_size);
	pr_info("[%s] dump command: %s\n", __func__, emi_dump_command);
	conndump_set_dump_state(ctx, CORE_DUMP_EMI);
	ret = conndump_netlink_send_to_native(ctx->conn_type, "[EMI]", emi_dump_command, strlen(emi_dump_command));

	if (ret < 0) {
		pr_err("Start EMI dump fail, ret = %d\n", ret);
		return -1;
	}

	comp_ret = wait_for_completion_timeout(
		&ctx->emi_dump,
		msecs_to_jiffies(CONNSYS_EMIDUMP_TIMEOUT));

	if (comp_ret == 0) {
		pr_err("EMI dump timeout\n");
		conndump_set_dump_state(ctx, CORE_DUMP_EMI_TIMEOUT);
	} else {
		pr_info("EMI dump end");
		conndump_set_dump_state(ctx, CORE_DUMP_DONE);
	}

	return 0;
}

static int connsys_coredump_init_dump_regions(struct connsys_dump_ctx* ctx, int num)
{
	int size = sizeof(struct dump_region)*num;

	ctx->dump_regions = (struct dump_region*)conndump_malloc(size);
	if (!ctx->dump_regions) {
		ctx->dump_regions_num = 0;
		pr_err("ctx->dump_regions create fail!\n");
		return 0;
	}
	memset(ctx->dump_regions, 0, size);
	ctx->dump_regions_num = num;
	return num;
}

static void connsys_coredump_deinit_dump_regions(struct connsys_dump_ctx* ctx)
{
	conndump_free(ctx->dump_regions);
	ctx->dump_regions = 0;
	ctx->dump_regions_num = 0;
}

static void conndump_coredump_end(void* handler)
{
	struct connsys_dump_ctx* ctx = (struct connsys_dump_ctx*)handler;

	if (conndump_get_dump_state(ctx) == CORE_DUMP_EMI) {
		pr_info("Wake up emi_dump\n");
		complete(&ctx->emi_dump);
	}
}

static int conndump_send_fake_coredump(struct connsys_dump_ctx* ctx)
{
	pr_info("Send fake coredump\n");
	return conndump_netlink_send_to_native(ctx->conn_type, "[M]", "FORCE_COREDUMP", 13);
}

static void conndump_exception_show(struct connsys_dump_ctx* ctx, bool full_dump)
{
	if (full_dump) {
		snprintf(
			ctx->info.exception_log, CONNSYS_AEE_INFO_SIZE,
			"%s %s %s %s",
			INFO_HEAD, ctx->hw_config.name,
			ctx->info.assert_type, ctx->info.dump_info);
	} else {
		snprintf(
			ctx->info.exception_log, CONNSYS_AEE_INFO_SIZE,
			"%s %s",
			INFO_HEAD, ctx->hw_config.name);
	}

#if defined(CONNINFRA_PLAT_ALPS) && CONNINFRA_PLAT_ALPS
	pr_info("par1: [%s] pars: [%s] par3: [%d]\n",
		ctx->hw_config.exception_tag_name,
		ctx->info.exception_log,
		strlen(ctx->info.exception_log));
	/* Call AEE API */
	aed_common_exception_api(
		ctx->hw_config.exception_tag_name,
		NULL, 0,
		(const int*)ctx->info.exception_log, strlen(ctx->info.exception_log),
		ctx->info.exception_log, 0);
#endif
}

/*****************************************************************************
 * FUNCTION
 *  connsys_coredump_clean
 * DESCRIPTION
 *  To clean coredump EMI region
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
void connsys_coredump_clean(void* handler)
{
	struct connsys_dump_ctx* ctx = (struct connsys_dump_ctx*)handler;

	if (ctx == NULL)
		return;

	pr_info("[%s] Clear %p size=%d as zero\n", __func__, ctx->emi_virt_addr_base, ctx->emi_size);
	memset_io(ctx->emi_virt_addr_base, 0, ctx->emi_size);

	conndump_set_dump_state(ctx, CORE_DUMP_INIT);
}
EXPORT_SYMBOL(connsys_coredump_clean);


/*****************************************************************************
 * FUNCTION
 *  connsys_coredump_setup_dump_region
 * DESCRIPTION
 *  Parse dump region in EMI (including CR regions, IDLM, SYSRAM, ...)
 *  The number would be record in ctx->dump_regions_num
 *  The data would be stored in ctx->dump_regions
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
int connsys_coredump_setup_dump_region(void* handler)
{
	int ret = 0;
	int cr_regions_idx = 0;
	int total_mem_region;
	int total_count;
	int i, idx = 0, offset;
	struct connsys_dump_ctx* ctx = (struct connsys_dump_ctx*)handler;
	struct dump_region* curr_region = 0;
	const unsigned int BUF_SIZE = 1024;
	const unsigned int MAX_IN_LINE = 10;
	char buf[BUF_SIZE];
	int wsize = 0;
	unsigned int accum_size = 0;
	unsigned int cr_count = 1;

	total_mem_region = conndump_get_dmp_info(
		ctx, CONNSYS_DUMP_CTRL_BLOCK_OFFSET + EXP_CTRL_TOTAL_MEM_REGION, false);

	/* Get idx first. The number is idx/8 */
	cr_regions_idx = conndump_get_dmp_info(
				ctx, CONNSYS_DUMP_CTRL_BLOCK_OFFSET + EXP_CTRL_CR_REGION_IDX, false);
	if (cr_regions_idx & 0x7) {
		pr_err("cr_regions_idx should be multiple of 8. But it is %d.\n", cr_regions_idx);
	}
	cr_regions_idx = (cr_regions_idx >> 3);
	total_count = total_mem_region + cr_regions_idx;
	pr_info("CR region=%d. Memory region=%d total dump regions is %d.\n",
		cr_regions_idx, total_mem_region, total_count);

	if (ctx->dump_regions) {
		connsys_coredump_deinit_dump_regions(ctx);
	}
	if (total_count == 0) {
		pr_info("Total dump regions is %d.\n", total_count);
		return 0;
	}

	ret = connsys_coredump_init_dump_regions(ctx, total_count);
	if (ret != total_count) {
		pr_err("[%s] allocate %d dump regions failed\n", __func__, total_count);
		return 0;
	}

	/* Setup memory region */
	offset = CONNSYS_DUMP_CTRL_BLOCK_OFFSET + EXP_CTRL_MEM_REGION_NAME_1;
	for (i = 0, curr_region = ctx->dump_regions; i < total_mem_region && idx < total_count; i++, idx++, offset += 12, curr_region++) {
		conndump_get_dmp_char(ctx, offset, 4, curr_region->name);
		curr_region->base = conndump_get_dmp_info(ctx, offset + 4, false);
		curr_region->length = conndump_get_dmp_info(ctx, offset + 8, false);
		pr_info("[%d][Memory region] name: %s, base: %x, length: %x\n",
			idx,
			curr_region->name,
			curr_region->base,
			curr_region->length);
	}

	memset(buf, '\0', sizeof(buf));
	wsize = snprintf(buf, BUF_SIZE, "[CR region](base,length): ");

	offset = CONNSYS_DUMP_CR_REGION_OFFSET;
	for (i = 0; i < cr_regions_idx && idx < total_count; i++, idx++, offset+=8) {
		ctx->dump_regions[idx].base = conndump_get_dmp_info(ctx, offset, false);
		ctx->dump_regions[idx].length = conndump_get_dmp_info(ctx, offset + 4, false);

		if (wsize >= 0 && wsize < (BUF_SIZE - accum_size)) {
			accum_size += wsize;
			wsize = snprintf(buf + accum_size, (BUF_SIZE - accum_size), "[%d](0x%x, %d), ", idx, ctx->dump_regions[idx].base, ctx->dump_regions[idx].length);
		}

		if (cr_count % MAX_IN_LINE == 0) {
			pr_info("%s", buf);
			memset(buf, '\0', sizeof(buf));
			wsize = snprintf(buf, BUF_SIZE, "[CR region](base,length): ");
			accum_size = 0;
		}

		cr_count++;
	}

	if ((cr_count - 1) % MAX_IN_LINE != 0) {
		pr_info("%s", buf);
	}

	return ctx->dump_regions_num;
}
EXPORT_SYMBOL(connsys_coredump_setup_dump_region);

/*****************************************************************************
 * FUNCTION
 *  connsys_coredump_start
 * DESCRIPTION
 *
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
int connsys_coredump_start(
	void* handler,
	unsigned int dump_property,
	int drv,
	char* reason)
{
	struct connsys_dump_ctx* ctx = (struct connsys_dump_ctx*)handler;
	int ret = 0;
	bool full_dump = false;
	struct timeval begin, end, put_done;
	struct timeval mem_start, mem_end, cr_start, cr_end, emi_dump_start, emi_dump_end;

	static DEFINE_RATELIMIT_STATE(_rs, HZ, 1);

	if (ctx == NULL)
		return 0;

	/* TODO: Check coredump mode */
	if (connsys_coredump_get_mode() == DUMP_MODE_RESET_ONLY) {
		pr_info("Chip reset only, skip coredump\n");
		return 0;
	}

	pr_info("[COREDUMP] dump_property=[0x%x] drv=[%d] reason=[%s]\n", dump_property, drv, reason);
	do_gettimeofday(&begin);
	conndump_set_dump_state(ctx, CORE_DUMP_START);
	/* Reset assert info */
	memset(&ctx->info, 0, sizeof(struct connsys_dump_info));
	if (drv >= CONNDRV_TYPE_BT && drv < CONNDRV_TYPE_MAX) {
		ctx->info.issue_type = CONNSYS_ISSUE_DRIVER_ASSERT;
		ctx->info.drv_type = drv;
		snprintf(ctx->info.reason, CONNSYS_ASSERT_REASON_SIZE, "%s", reason);
	}

	/* Start coredump timer */
	mod_timer(&ctx->dmp_timer, jiffies + (CONNSYS_COREDUMP_TIMEOUT) / (1000 / HZ));

	/* Check coredump status */
	while (1) {
		if (conndump_get_dmp_info(ctx, CONNSYS_DUMP_CTRL_BLOCK_OFFSET + EXP_CTRL_DUMP_STATE, false) == FW_DUMP_STATE_PUT_DONE) {
			pr_info("coredump put done\n");
			del_timer(&ctx->dmp_timer);
			full_dump = true;
			break;
		} else if (conndump_get_dump_state(ctx) == CORE_DUMP_TIMEOUT) {
			pr_err("Coredump timeout\n");
			if (ctx->callback.poll_cpupcr) {
				pr_info("Debug dump:\n");
				ctx->callback.poll_cpupcr(5, 1);
			}
			conndump_send_fake_coredump(ctx);
			goto partial_dump;
		}
		if (__ratelimit(&_rs)) {
			pr_info("Wait coredump state, EMI[0]=0x%x EMI[4]=0x%x\n",
				conndump_get_dmp_info(ctx, 0, false),
				conndump_get_dmp_info(ctx, 4, false));
		}
		if (dump_property & CONNSYS_DUMP_PROPERTY_NO_WAIT) {
			pr_info("Don't wait dump status, go to partial dump\n");
			if (ctx->callback.poll_cpupcr) {
				pr_info("Debug dump:\n");
				ctx->callback.poll_cpupcr(5, 1);
			}
			conndump_send_fake_coredump(ctx);
			goto partial_dump;
		}
		msleep(1);
	}
	do_gettimeofday(&put_done);
	conndump_set_dump_state(ctx, CORE_DUMP_DOING);

	/* Get print_buff */
	conndump_dump_print_buff(ctx);

	/* Get dump_buff and send to pack it */
	conndump_dump_dump_buff(ctx);

partial_dump:
	/* TODO: move to init or other suitable place */
	ret = connsys_coredump_setup_dump_region(ctx);
	if (!ret)
		pr_err("No dump region found\n");

	conndump_set_dump_state(ctx, CORE_DUMP_DOING);
	/* Memory region and CR region should be setup when MCU init */
	/* Parse CR region and send to pack it */
	do_gettimeofday(&cr_start);
	conndump_dump_cr_regions(ctx);
	do_gettimeofday(&cr_end);

	/* Construct assert info and send to native */
	conndump_info_format(ctx, ctx->page_dump_buf, CONNSYS_DUMP_BUFF_SIZE, full_dump);

	/* Read memory region on ctrl block */
	do_gettimeofday(&mem_start);
	conndump_dump_mem_regions(ctx);
	do_gettimeofday(&mem_end);


	/* Start EMI dump */
	do_gettimeofday(&emi_dump_start);
	conndump_dump_emi(ctx);
	do_gettimeofday(&emi_dump_end);

	/* All process finished, set to init status */
	conndump_set_dump_state(ctx, CORE_DUMP_INIT);

	conndump_exception_show(ctx, full_dump);
	do_gettimeofday(&end);
	pr_info("Coredump end\n");
	if (full_dump) {
		pr_info("%s coredump summary: full dump total=[%lu] put_done=[%lu] cr=[%lu] mem=[%lu] emi=[%lu]\n",
			g_type_name[ctx->conn_type],
			timeval_to_ms(&begin, &end),
			timeval_to_ms(&begin, &put_done),
			timeval_to_ms(&cr_start, &cr_end),
			timeval_to_ms(&mem_start, &mem_end),
			timeval_to_ms(&emi_dump_start, &emi_dump_end));
	} else {
		pr_info("%s coredump summary: partial dump total=[%lu] cr=[%lu] mem=[%lu] emi=[%lu]\n",
			g_type_name[ctx->conn_type],
			timeval_to_ms(&begin, &end),
			timeval_to_ms(&cr_start, &cr_end),
			timeval_to_ms(&mem_start, &mem_end),
			timeval_to_ms(&emi_dump_start, &emi_dump_end));
	}
	return 0;
}
EXPORT_SYMBOL(connsys_coredump_start);

/*****************************************************************************
 * FUNCTION
 *  connsys_coredump_get_start_offset
 * DESCRIPTION
 *
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
phys_addr_t connsys_coredump_get_start_offset(int conn_type)
{
	struct coredump_hw_config *config = get_coredump_platform_config(conn_type);

	if (config)
		return config->start_offset;
	return 0;
}
EXPORT_SYMBOL(connsys_coredump_get_start_offset);

/*****************************************************************************
 * FUNCTION
 *  connsys_coredump_init
 * DESCRIPTION
 *
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
void* connsys_coredump_init(
	int conn_type,
	struct coredump_event_cb* cb)
{
	struct connsys_dump_ctx* ctx = 0;
	struct netlink_event_cb nl_cb;
	struct coredump_hw_config *config = get_coredump_platform_config(conn_type);
	phys_addr_t emi_base;
	unsigned int emi_size, mcif_emi_size;

	/* Get EMI config */
	if (config == NULL) {
		pr_err("Get coredump EMI config fail\n");
		goto error_exit;
	}

	ctx = (struct connsys_dump_ctx*)conndump_malloc(sizeof(struct connsys_dump_ctx));
	if (!ctx) {
		pr_err("Allocate connsys_dump_ctx fail");
		goto error_exit;
	}

	/* clean it */
	memset(ctx, 0, sizeof(struct connsys_dump_ctx));
	memcpy(&ctx->hw_config, config, sizeof(struct coredump_hw_config));

	ctx->page_dump_buf = (char*)conndump_malloc(CONNSYS_DUMP_BUFF_SIZE);
	if (!ctx->page_dump_buf) {
		pr_err("Create dump buffer fail.\n");
		goto error_exit;
	}

	ctx->conn_type = conn_type;
	if (cb)
		memcpy(&ctx->callback, cb, sizeof(struct coredump_event_cb));
	else
		pr_info("[%s][%d] callback is null\n", __func__, conn_type);

	/* EMI init */
	conninfra_get_emi_phy_addr(CONNSYS_EMI_FW, &emi_base, &emi_size);
	conninfra_get_emi_phy_addr(CONNSYS_EMI_MCIF, NULL, &mcif_emi_size);
	pr_info("Get emi_base=0x%x emi_size=%d\n", emi_base, emi_size);
	ctx->full_emi_size = emi_size;
	ctx->emi_phy_addr_base = config->start_offset + emi_base;
	ctx->emi_size = config->size;
	ctx->mcif_emi_size = mcif_emi_size;

	ctx->emi_virt_addr_base =
		ioremap_nocache(ctx->emi_phy_addr_base, ctx->emi_size);
	if (ctx->emi_virt_addr_base == 0) {
		pr_err("Remap emi fail (0x%08x) size=%d",
			ctx->emi_phy_addr_base, ctx->emi_size);
		goto error_exit;
	}

	pr_info("Clear %p size=%d as zero\n", ctx->emi_virt_addr_base, ctx->emi_size);
	memset_io(ctx->emi_virt_addr_base, 0, ctx->emi_size);

	/* Setup timer */
	init_timer(&ctx->dmp_timer);
	ctx->dmp_timer.function = conndump_timeout_handler;
	ctx->dmp_timer.data = (unsigned long)ctx;
	init_completion(&ctx->emi_dump);

	conndump_set_dump_state(ctx, CORE_DUMP_INIT);

	/* Init netlink */
	nl_cb.coredump_end = conndump_coredump_end;
	conndump_netlink_init(ctx->conn_type, ctx, &nl_cb);
	return ctx;

error_exit:
	if (ctx)
		connsys_coredump_deinit(ctx);

	return 0;
}
EXPORT_SYMBOL(connsys_coredump_init);

/*****************************************************************************
 * FUNCTION
 *  connsys_coredump_deinit
 * DESCRIPTION
 *
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
void connsys_coredump_deinit(void* handler)
{
	struct connsys_dump_ctx* ctx = (struct connsys_dump_ctx*)handler;

	if (handler == NULL)
		return;

	if (ctx->emi_virt_addr_base) {
		iounmap(ctx->emi_virt_addr_base);
		ctx->emi_virt_addr_base = NULL;
	}

	if (ctx->page_dump_buf) {
		conndump_free(ctx->page_dump_buf);
		ctx->page_dump_buf = NULL;
	}

	if (ctx->dump_regions) {
		connsys_coredump_deinit_dump_regions(ctx);
		ctx->dump_regions = NULL;
	}

	conndump_free(ctx);
}
EXPORT_SYMBOL(connsys_coredump_deinit);


/*****************************************************************************
 * FUNCTION
 *  connsys_coredump_get_mode
 * DESCRIPTION
 *
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
enum connsys_coredump_mode connsys_coredump_get_mode(void)
{
	return atomic_read(&g_dump_mode);
}
EXPORT_SYMBOL(connsys_coredump_get_mode);

/*****************************************************************************
 * FUNCTION
 *  connsys_coredump_set_dump_mode
 * DESCRIPTION
 *
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
void connsys_coredump_set_dump_mode(enum connsys_coredump_mode mode)
{
	if (mode < DUMP_MODE_MAX)
		atomic_set(&g_dump_mode, mode);
}
EXPORT_SYMBOL(connsys_coredump_set_dump_mode);


