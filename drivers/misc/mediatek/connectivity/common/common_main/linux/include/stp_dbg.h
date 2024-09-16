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

#ifndef _STP_DEBUG_H_
#define _STP_DEBUG_H_

#include <linux/time.h>
#include "stp_btif.h"
#include "osal.h"
#include "wmt_exp.h"

#define CONFIG_LOG_STP_INTERNAL

#ifndef LOG_STP_DEBUG_DISABLE		/* #ifndef CONFIG_LOG_STP_INTERNAL */
#define STP_PKT_SZ  16
#define STP_DMP_SZ 2048
#define STP_PKT_NO 2048

#define STP_DBG_LOG_ENTRY_NUM	1024
#define STP_DBG_LOG_ENTRY_SZ	96

#else

#define STP_PKT_SZ  16
#define STP_DMP_SZ 16
#define STP_PKT_NO 16

#define STP_DBG_LOG_ENTRY_NUM 28
#define STP_DBG_LOG_ENTRY_SZ 96

#endif
#define EMICOREDUMP_CMD "emicoredump"
#define FAKECOREDUMPEND "coredump end - fake"

#define MAX_DUMP_HEAD_LEN 512
/* netlink header packet length is 5 "[M](3 bytes) + length(2 bypes)" */
#define NL_PKT_HEADER_LEN 5

#define PFX_STP_DBG                      "[STPDbg]"
#define STP_DBG_LOG_LOUD                 4
#define STP_DBG_LOG_DBG                  3
#define STP_DBG_LOG_INFO                 2
#define STP_DBG_LOG_WARN                 1
#define STP_DBG_LOG_ERR                  0

extern INT32 gStpDbgDbgLevel;

#define STP_DBG_PR_LOUD(fmt, arg...) \
do { \
	if (gStpDbgDbgLevel >= STP_DBG_LOG_LOUD) \
		pr_info(PFX_STP_DBG "%s: "  fmt, __func__, ##arg); \
} while (0)
#define STP_DBG_PR_DBG(fmt, arg...) \
do { \
	if (gStpDbgDbgLevel >= STP_DBG_LOG_DBG) \
		pr_info(PFX_STP_DBG "%s: "  fmt, __func__, ##arg); \
} while (0)
#define STP_DBG_PR_INFO(fmt, arg...) \
do { \
	if (gStpDbgDbgLevel >= STP_DBG_LOG_INFO) \
		pr_info(PFX_STP_DBG "%s: "  fmt, __func__, ##arg); \
} while (0)
#define STP_DBG_PR_WARN(fmt, arg...) \
do { \
	if (gStpDbgDbgLevel >= STP_DBG_LOG_WARN) \
		pr_warn(PFX_STP_DBG "%s: "  fmt, __func__, ##arg); \
} while (0)
#define STP_DBG_PR_ERR(fmt, arg...) \
do { \
	if (gStpDbgDbgLevel >= STP_DBG_LOG_ERR) \
		pr_err(PFX_STP_DBG "%s: "   fmt, __func__, ##arg); \
} while (0)

typedef enum {
	STP_DBG_EN = 0,
	STP_DBG_PKT = 1,
	STP_DBG_DR = 2,
	STP_DBG_FW_ASSERT = 3,
	STP_DBG_FW_LOG = 4,
	STP_DBG_FW_DMP = 5,
	STP_DBG_MAX
} STP_DBG_OP_T;

typedef enum {
	STP_DBG_PKT_FIL_ALL = 0,
	STP_DBG_PKT_FIL_BT = 1,
	STP_DBG_PKT_FIL_GPS = 2,
	STP_DBG_PKT_FIL_FM = 3,
	STP_DBG_PKT_FIL_WMT = 4,
	STP_DBG_PKT_FIL_MAX
} STP_DBG_PKT_FIL_T;

static PINT8 const comboStpDbgType[] = {
	"< BT>",
	"< FM>",
	"<GPS>",
	"<WiFi>",
	"<WMT>",
	"<STP>",
	"<DBG>",
	"<ANT>",
	"<SDIO_OWN_SET>",
	"<SDIO_OWN_CLR>",
	"<WAKEINT>",
	"<UNKNOWN>"
};

static PINT8 const socStpDbgType[] = {
	"< BT>",
	"< FM>",
	"<GPS>",
	"<WiFi>",
	"<WMT>",
	"<STP>",
	"<GPSL5>",
	"<WAKEINT>",
	"<UNKNOWN>"
};

enum STP_DBG_TAKS_ID_T {
	STP_DBG_TASK_WMT = 0,
	STP_DBG_TASK_BT,
	STP_DBG_TASK_WIFI,
	STP_DBG_TASK_TST,
	STP_DBG_TASK_FM,
	STP_DBG_TASK_GPS,
	STP_DBG_TASK_FLP,
	STP_DBG_TASK_BT2,
	STP_DBG_TASK_IDLE,
	STP_DBG_TASK_DRVSTP,
	STP_DBG_TASK_BUS,
	STP_DBG_TASK_NATBT,
	STP_DBG_TASK_DRVWIFI,
	STP_DBG_TASK_DRVGPS,
	STP_DBG_TASK_ID_MAX,
};

typedef enum {
	STP_DBG_DR_MAX = 0,
} STP_DBG_DR_FIL_T;

typedef enum {
	STP_DBG_FW_MAX = 0,
} STP_DBG_FW_FIL_T;

typedef enum {
	PKT_DIR_RX = 0,
	PKT_DIR_TX
} STP_DBG_PKT_DIR_T;

/*simple log system ++*/

typedef struct {
	/*type: 0. pkt trace 1. fw info
	* 2. assert info 3. trace32 dump .
	* -1. linked to the the previous
	*/
	INT32 id;
	INT32 len;
	INT8 buffer[STP_DBG_LOG_ENTRY_SZ];
} MTKSTP_LOG_ENTRY_T;

typedef struct log_sys {
	MTKSTP_LOG_ENTRY_T queue[STP_DBG_LOG_ENTRY_NUM];
	UINT32 size;
	UINT32 in;
	UINT32 out;
	spinlock_t lock;
	MTKSTP_LOG_ENTRY_T *dump_queue;
	UINT32 dump_size;
	struct work_struct dump_work;
} MTKSTP_LOG_SYS_T;
/*--*/

typedef struct stp_dbg_pkt_hdr {
	/* packet information */
	UINT32 sec;
	UINT32 usec;
	UINT32 dbg_type;
	UINT32 last_dbg_type;
	UINT32 dmy;
	UINT32 no;
	UINT32 dir;

	/* packet content */
	UINT32 type;
	UINT32 len;
	UINT32 ack;
	UINT32 seq;
	UINT32 chs;
	UINT32 crc;
	UINT64 l_sec;
	ULONG l_nsec;
} STP_DBG_HDR_T;

typedef struct stp_dbg_pkt {
	struct stp_dbg_pkt_hdr hdr;
	UINT8 raw[STP_DMP_SZ];
} STP_PACKET_T;

typedef struct mtkstp_dbg_t {
	/*log_sys */
	INT32 pkt_trace_no;
	PVOID btm;
	INT32 is_enable;
	MTKSTP_LOG_SYS_T *logsys;
} MTKSTP_DBG_T;

/* extern void aed_combo_exception(const int *, int, const int *, int, const char *); */
#if WMT_DBG_SUPPORT
#define STP_CORE_DUMP_TIMEOUT (1*60*1000)	/* default 1 minutes */
#else
#define STP_CORE_DUMP_TIMEOUT (10*1000)		/* user load default 10 seconds */
#endif
#if WMT_DBG_SUPPORT
#define STP_EMI_DUMP_TIMEOUT  (30*1000)
#else
#define STP_EMI_DUMP_TIMEOUT  (5*1000)
#endif
#define STP_OJB_NAME_SZ 20
#define STP_CORE_DUMP_INFO_SZ 500
#define STP_CORE_DUMP_INIT_SIZE 512
typedef enum wcn_compress_algorithm_t {
	GZIP = 0,
	BZIP2 = 1,
	RAR = 2,
	LMA = 3,
	MAX
} WCN_COMPRESS_ALG_T;

typedef INT32 (*COMPRESS_HANDLER) (PVOID worker, UINT8 *in_buf, INT32 in_sz, PUINT8 out_buf, PINT32 out_sz,
				  INT32 finish);
typedef struct wcn_compressor_t {
	/* current object name */
	UINT8 name[STP_OJB_NAME_SZ + 1];

	/* buffer for raw data, named L1 */
	PUINT8 L1_buf;
	INT32 L1_buf_sz;
	INT32 L1_pos;

	/* target buffer, named L2 */
	PUINT8 L2_buf;
	INT32 L2_buf_sz;
	INT32 L2_pos;

	/* compress state */
	UINT8 f_done;
	UINT16 reserved;
	UINT32 uncomp_size;
	UINT32 crc32;

	/* compress algorithm */
	UINT8 f_compress_en;
	WCN_COMPRESS_ALG_T compress_type;
	PVOID worker;
	COMPRESS_HANDLER handler;
} WCN_COMPRESSOR_T, *P_WCN_COMPRESSOR_T;

typedef enum core_dump_state_t {
	CORE_DUMP_INIT = 0,
	CORE_DUMP_DOING,
	CORE_DUMP_TIMEOUT,
	CORE_DUMP_DONE,
	CORE_DUMP_MAX
} CORE_DUMP_STA;

typedef struct core_dump_t {
	/* compress dump data and buffered */
	P_WCN_COMPRESSOR_T compressor;

	/* timer for monitor timeout */
	OSAL_TIMER dmp_timer;
	UINT32 timeout;
	LONG dmp_num;
	UINT32 count;
	OSAL_SLEEPABLE_LOCK dmp_lock;

	/* timer for monitor emi dump */
	OSAL_TIMER dmp_emi_timer;
	UINT32 emi_timeout;

	/* state machine for core dump flow */
	CORE_DUMP_STA sm;

	/* dump info */
	INT8 info[STP_CORE_DUMP_INFO_SZ + 1];

	PUINT8 p_head;
	UINT32 head_len;
} WCN_CORE_DUMP_T, *P_WCN_CORE_DUMP_T;

typedef enum _ENUM_STP_FW_ISSUE_TYPE_ {
	STP_FW_ISSUE_TYPE_INVALID = 0x0,
	STP_FW_ASSERT_ISSUE = 0x1,
	STP_FW_NOACK_ISSUE = 0x2,
	STP_FW_WARM_RST_ISSUE = 0x3,
	STP_DBG_PROC_TEST = 0x4,
	STP_HOST_TRIGGER_FW_ASSERT = 0x5,
	STP_HOST_TRIGGER_ASSERT_TIMEOUT = 0x6,
	STP_FW_ABT = 0x7,
	STP_HOST_TRIGGER_COLLECT_FTRACE = 0x8,
	STP_FW_ISSUE_TYPE_MAX
} ENUM_STP_FW_ISSUE_TYPE, *P_ENUM_STP_FW_ISSUE_TYPE;

/* this was added for support dmareg's issue */
typedef enum _ENUM_DMA_ISSUE_TYPE_ {
	CONNSYS_CLK_GATE_STATUS = 0x00,
	CONSYS_EMI_STATUS,
	SYSRAM1,
	SYSRAM2,
	SYSRAM3,
	DMA_REGS_MAX
} ENUM_DMA_ISSUE_TYPE;
#define STP_PATCH_TIME_SIZE 12
#define STP_DBG_CPUPCR_NUM 30
#define STP_DBG_DMAREGS_NUM 16
#define STP_PATCH_BRANCH_SZIE 8
#define STP_ASSERT_INFO_SIZE 164
#define STP_DBG_ROM_VER_SIZE 4
#define STP_ASSERT_TYPE_SIZE 64

#define STP_DBG_KEYWORD_SIZE 256
typedef struct stp_dbg_host_assert_t {
	UINT32 drv_type;
	UINT32 reason;
	UINT32 assert_from_host;
} STP_DBG_HOST_ASSERT_T, *P_STP_DBG_HOST_ASSERT_T;

typedef struct stp_dbg_cpupcr_t {
	UINT32 chipId;
	UINT8 romVer[STP_DBG_ROM_VER_SIZE];
	UINT8 patchVer[STP_PATCH_TIME_SIZE];
	UINT8 branchVer[STP_PATCH_BRANCH_SZIE];
	UINT32 wifiVer;
	UINT32 count;
	UINT32 stop_flag;
	UINT32 buffer[STP_DBG_CPUPCR_NUM];
	UINT64 sec_buffer[STP_DBG_CPUPCR_NUM];
	ULONG nsec_buffer[STP_DBG_CPUPCR_NUM];
	UINT8 assert_info[STP_ASSERT_INFO_SIZE];
	UINT32 fwTaskId;
	UINT32 fwRrq;
	UINT32 fwIsr;
	STP_DBG_HOST_ASSERT_T host_assert_info;
	UINT8 assert_type[STP_ASSERT_TYPE_SIZE];
	ENUM_STP_FW_ISSUE_TYPE issue_type;
	UINT8 keyword[STP_DBG_KEYWORD_SIZE];
	OSAL_SLEEPABLE_LOCK lock;
} STP_DBG_CPUPCR_T, *P_STP_DBG_CPUPCR_T;

typedef struct stp_dbg_dmaregs_t {
	UINT32 count;
	UINT32 dmaIssue[DMA_REGS_MAX][STP_DBG_DMAREGS_NUM];
	OSAL_SLEEPABLE_LOCK lock;
} STP_DBG_DMAREGS_T, *P_STP_DBG_DMAREGS_T;

typedef enum _ENUM_ASSERT_INFO_PARSER_TYPE_ {
	STP_DBG_ASSERT_INFO = 0x0,
	STP_DBG_FW_TASK_ID = 0x1,
	STP_DBG_FW_ISR = 0x2,
	STP_DBG_FW_IRQ = 0x3,
	STP_DBG_ASSERT_TYPE = 0x4,
	STP_DBG_PARSER_TYPE_MAX
} ENUM_ASSERT_INFO_PARSER_TYPE, *P_ENUM_ASSERT_INFO_PARSER_TYPE;

VOID stp_dbg_nl_init(VOID);
VOID stp_dbg_nl_deinit(VOID);
INT32 stp_dbg_core_dump_deinit_gcoredump(VOID);
INT32 stp_dbg_core_dump_flush(INT32 rst, MTK_WCN_BOOL coredump_is_timeout);
INT32 stp_dbg_core_dump(INT32 dump_sink);
INT32 stp_dbg_trigger_collect_ftrace(PUINT8 pbuf, INT32 len);
#if BTIF_RXD_BE_BLOCKED_DETECT
MTK_WCN_BOOL stp_dbg_is_btif_rxd_be_blocked(VOID);
#endif
INT32 stp_dbg_enable(MTKSTP_DBG_T *stp_dbg);
INT32 stp_dbg_disable(MTKSTP_DBG_T *stp_dbg);
INT32 stp_dbg_dmp_print(MTKSTP_DBG_T *stp_dbg);
INT32 stp_dbg_dmp_out(MTKSTP_DBG_T *stp_dbg, PINT8 buf, PINT32 len);
INT32 stp_dbg_dmp_append(MTKSTP_DBG_T *stp_dbg, PUINT8 pBuf, INT32 max_len);

INT32 stp_dbg_dmp_out_ex(PINT8 buf, PINT32 len);
INT32 stp_dbg_log_pkt(MTKSTP_DBG_T *stp_dbg, INT32 dbg_type,
		      INT32 type, INT32 ack_no, INT32 seq_no, INT32 crc, INT32 dir, INT32 len,
		      const PUINT8 body);
INT32 stp_dbg_log_ctrl(UINT32 on);
INT32 stp_dbg_aee_send(PUINT8 aucMsg, INT32 len, INT32 cmd);
INT32 stp_dbg_dump_num(LONG dmp_num);
INT32 stp_dbg_nl_send(PINT8 aucMsg, UINT8 cmd, INT32 len);
INT32 stp_dbg_dump_send_retry_handler(PINT8 tmp, INT32 len);
VOID stp_dbg_set_coredump_timer_state(CORE_DUMP_STA state);
INT32 stp_dbg_get_coredump_timer_state(VOID);
INT32 stp_dbg_poll_cpupcr(UINT32 times, UINT32 sleep, UINT32 cmd);
INT32 stp_dbg_poll_dmaregs(UINT32 times, UINT32 sleep);
INT32 stp_dbg_poll_cpupcr_ctrl(UINT32 en);
INT32 stp_dbg_set_version_info(UINT32 chipid, PUINT8 pRomVer, PUINT8 pPatchVer, PUINT8 pPatchBrh);
INT32 stp_dbg_set_wifiver(UINT32 wifiver);
INT32 stp_dbg_set_host_assert_info(UINT32 drv_type, UINT32 reason, UINT32 en);
VOID stp_dbg_set_keyword(PINT8 keyword);
UINT32 stp_dbg_get_host_trigger_assert(VOID);
INT32 stp_dbg_set_fw_info(PUINT8 issue_info, UINT32 len, ENUM_STP_FW_ISSUE_TYPE issue_type);
INT32 stp_dbg_cpupcr_infor_format(PUINT8 buf, UINT32 max_len);
INT32 stp_dbg_dump_cpupcr_reg_info(PUINT8 buf, UINT32 consys_lp_reg);
VOID stp_dbg_clear_cpupcr_reg_info(VOID);
PUINT8 stp_dbg_id_to_task(UINT32 id);
VOID stp_dbg_reset(VOID);

MTKSTP_DBG_T *stp_dbg_init(PVOID btm_half);
INT32 stp_dbg_deinit(MTKSTP_DBG_T *stp_dbg);
INT32 stp_dbg_start_coredump_timer(VOID);
INT32 stp_dbg_start_emi_dump(VOID);
INT32 stp_dbg_stop_emi_dump(VOID);
INT32 stp_dbg_nl_send_data(const PINT8 buf, INT32 len);
#endif /* end of _STP_DEBUG_H_ */
