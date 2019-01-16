#ifndef _STP_DEBUG_H_
#define _STP_DEBUG_H_

#include <linux/time.h>
#include "osal.h"

#define CONFIG_LOG_STP_INTERNAL

#if 1				/* #ifndef CONFIG_LOG_STP_INTERNAL */
#define STP_PKT_SZ  16
#define STP_DMP_SZ 2048
#define STP_PKT_NO 2048

#define STP_DBG_LOG_ENTRY_NUM 1024
#define STP_DBG_LOG_ENTRY_SZ  2048

#else

#define STP_PKT_SZ  16
#define STP_DMP_SZ 16
#define STP_PKT_NO 16

#define STP_DBG_LOG_ENTRY_NUM 28
#define STP_DBG_LOG_ENTRY_SZ 64


#endif


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

static PINT8 const gStpDbgType[]={
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
	"<UNKOWN>"
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
	INT32 id;			/*type: 0. pkt trace 1. fw info 2. assert info 3. trace32 dump . -1. linked to the the previous */
	INT32 len;
	INT8 buffer[STP_DBG_LOG_ENTRY_SZ];
} MTKSTP_LOG_ENTRY_T;

typedef struct log_sys {
	MTKSTP_LOG_ENTRY_T queue[STP_DBG_LOG_ENTRY_NUM];
	UINT32 size;
	UINT32 in;
	UINT32 out;
	spinlock_t lock;
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


#define STP_CORE_DUMP_TIMEOUT 1*60*1000	/* default 5minutes */
#define STP_OJB_NAME_SZ 20
#define STP_CORE_DUMP_INFO_SZ 500
typedef enum wcn_compress_algorithm_t {
	GZIP = 0,
	BZIP2 = 1,
	RAR = 2,
	LMA = 3,
	MAX
} WCN_COMPRESS_ALG_T;

typedef INT32(*COMPRESS_HANDLER) (PVOID worker, PUINT8 in_buf, INT32 in_sz, PUINT8 out_buf,
				  PINT32 out_sz, INT32 finish);
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

P_WCN_COMPRESSOR_T wcn_compressor_init(PUINT8 name, INT32 L1_buf_sz, INT32 L2_buf_sz);
INT32 wcn_compressor_deinit(P_WCN_COMPRESSOR_T compressor);
INT32 wcn_compressor_in(P_WCN_COMPRESSOR_T compressor, PUINT8 buf, INT32 len, INT32 finish);
INT32 wcn_compressor_out(P_WCN_COMPRESSOR_T compressor, PPUINT8 pbuf, PINT32 len);
INT32 wcn_compressor_reset(P_WCN_COMPRESSOR_T compressor, UINT8 enable, WCN_COMPRESS_ALG_T type);

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

	OSAL_SLEEPABLE_LOCK dmp_lock;

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
    STP_FW_ABT = 0x6,
	STP_FW_ISSUE_TYPE_MAX
} ENUM_STP_FW_ISSUE_TYPE, *P_ENUM_STP_FW_ISSUE_TYPE;

#define STP_PATCH_TIME_SIZE 12
#define STP_DBG_CPUPCR_NUM 512
#define STP_PATCH_BRANCH_SZIE 8
#define STP_ASSERT_INFO_SIZE 64
#define STP_DBG_WIFI_VER_SIZE 8
#define STP_DBG_ROM_VER_SIZE 4
#define STP_ASSERT_TYPE_SIZE 32

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
	UINT8 wifiVer[STP_DBG_WIFI_VER_SIZE];
	UINT32 count;
	UINT32 stop_flag;
	UINT32 buffer[STP_DBG_CPUPCR_NUM];
	UINT8 assert_info[STP_ASSERT_INFO_SIZE];
	UINT32 fwTaskId;
	UINT32 fwRrq;
	UINT32 fwIsr;
	STP_DBG_HOST_ASSERT_T host_assert_info;
	UINT8 assert_type[STP_ASSERT_TYPE_SIZE];
	ENUM_STP_FW_ISSUE_TYPE issue_type;
	OSAL_SLEEPABLE_LOCK lock;
} STP_DBG_CPUPCR_T, *P_STP_DBG_CPUPCR_T;

typedef enum _ENUM_ASSERT_INFO_PARSER_TYPE_ {
	STP_DBG_ASSERT_INFO = 0x0,
	STP_DBG_FW_TASK_ID = 0x1,
	STP_DBG_FW_ISR = 0x2,
	STP_DBG_FW_IRQ = 0x3,
	STP_DBG_ASSERT_TYPE = 0x4,
	STP_DBG_PARSER_TYPE_MAX
} ENUM_ASSERT_INFO_PARSER_TYPE, *P_ENUM_ASSERT_INFO_PARSER_TYPE;

P_WCN_CORE_DUMP_T wcn_core_dump_init(UINT32 timeout);
INT32 wcn_core_dump_deinit(P_WCN_CORE_DUMP_T dmp);
INT32 wcn_core_dump_in(P_WCN_CORE_DUMP_T dmp, PUINT8 buf, INT32 len);
INT32 wcn_core_dump_out(P_WCN_CORE_DUMP_T dmp, PPUINT8 pbuf, PINT32 len);
INT32 wcn_core_dump_reset(P_WCN_CORE_DUMP_T dmp, UINT32 timeout);
extern INT32 wcn_core_dump_flush(INT32 rst);

extern int stp_dbg_enable(MTKSTP_DBG_T *stp_dbg);
extern int stp_dbg_disable(MTKSTP_DBG_T *stp_dbg);
extern MTKSTP_DBG_T *stp_dbg_init(PVOID);
extern int stp_dbg_deinit(MTKSTP_DBG_T *stp_dbg);
extern int stp_dbg_dmp_out_ex(PINT8 buf, PINT32 len);
extern int stp_dbg_dmp_out(MTKSTP_DBG_T *stp_dbg, PINT8 buf, PINT32 len);
extern int stp_dbg_dmp_print(MTKSTP_DBG_T *stp_dbg);
extern INT32 stp_dbg_nl_send(PINT8 aucMsg, UINT8 cmd, INT32 len);

extern INT32 stp_dbg_aee_send(PUINT8 aucMsg, INT32 len, INT32 cmd);

extern INT32
stp_dbg_log_pkt(MTKSTP_DBG_T *stp_dbg,
		INT32 dbg_type,
		INT32 type,
		INT32 ack_no, INT32 seq_no, INT32 crc, INT32 dir, INT32 len, const PUINT8 body);
extern int stp_dbg_log_ctrl(UINT32 on);
extern INT32 stp_dbg_set_version_info(UINT32 chipid, PUINT8 pRomVer, PUINT8 wifiVer,
				      PUINT8 pPatchVer, PUINT8 pPatchBrh);
extern INT32 stp_dbg_cpupcr_infor_format(PPUINT8 buf, PUINT32 len);
extern INT32 stp_dbg_set_fw_info(PUINT8 assert_info, UINT32 len,
				 ENUM_STP_FW_ISSUE_TYPE issue_type);
extern INT32 stp_dbg_set_host_assert_info(UINT32 drv_type, UINT32 reason, UINT32 en);

#endif				/* end of _STP_DEBUG_H_ */
