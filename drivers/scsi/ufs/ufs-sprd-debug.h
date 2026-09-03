/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#ifndef _UFS_SPRD_DEBUG_H_
#define _UFS_SPRD_DEBUG_H_

#ifdef CONFIG_SPRD_DEBUG
/* Userdebug ver default ON!!! */
#define UFS_DEBUG_ON_DEF true
#define UFS_DEBUG_ERR_PANIC_DEF true
#else
#define UFS_DEBUG_ON_DEF false
#define UFS_DEBUG_ERR_PANIC_DEF false
#endif

#define UFS_CMD_RECORD_DEPTH		500
#define UFS_SINGLE_LINE_STR_LIMIT	230
#define CMD_DUMP_BUFFER_S		(UFS_CMD_RECORD_DEPTH * UFS_SINGLE_LINE_STR_LIMIT)

#define PERF_RECORD_LINES_PER_RECORD	6
#define UFS_PERF_RECORD_DEPTH		360	/* loop to trace UFS perf data for 1 hours */
#define PERF_DUMP_BUFFER_S		(PERF_RECORD_LINES_PER_RECORD * UFS_PERF_RECORD_DEPTH *\
						UFS_SINGLE_LINE_STR_LIMIT)
#define MAX_UFS_DBG_HIST		5
#define DBG_HIST_DEPTH			600
#define DBG_DUMP_BUFFER_S		(MAX_UFS_DBG_HIST * DBG_HIST_DEPTH *\
						UFS_SINGLE_LINE_STR_LIMIT)
#define DUMP_BUFFER_S (UFS_CMD_RECORD_DEPTH * UFS_SINGLE_LINE_STR_LIMIT)

#define	THROUGHPUT_MIN_LEN SZ_1M

struct ufs_dbg_pkg {
	ktime_t time;
	u32 id;
	u32 data;
	bool preempt;
	bool active_uic_cmd;
	void *val_array;
};

struct ufs_dbg_hist {
	u32 pos;
	void *name_array;
	u32 each_pkg_num;
	struct ufs_dbg_pkg pkg[MAX_UFS_DBG_HIST];
};

enum ufs_event_list {
	UFS_TRACE_SEND,
	UFS_TRACE_COMPLETED,
	UFS_TRECE_SCSI_TIME_OUT,
	/* QUERY \ NOP_OUT&IN \ REJECT CMD */
	UFS_TRACE_DEV_SEND,
	UFS_TRACE_DEV_COMPLETED,
	UFS_TRACE_TM_SEND,
	UFS_TRACE_TM_COMPLETED,
	UFS_TRACE_TM_ERR,
	UFS_TRACE_UIC_SEND,
	UFS_TRACE_UIC_CMPL,
	UFS_TRACE_CLK_GATE,
	UFS_TRACE_EVT,

	/* The following events dont need additional parameters */
	UFS_TRACE_RESET_AND_RESTORE,
	UFS_TRACE_INT_ERROR,
	UFS_TRACE_DEBUG_TRIGGER,

	UFS_MAX_EVENT,
};

struct ufs_cmd_info {
	u8 opcode;
	u8 lun;
	u8 crypto_en;
	u8 keyslot;
	u32 tag;
	u32 transfer_len;
	sector_t lba;
	bool fua;
	u64 time_cost;
	unsigned short cmd_len;
	char cmnd[UFS_CDB_SIZE];

	/* response info */
	int ocs;
	int trans_type;
	int scsi_stat;
	int sd_size;
	char sense_data[UFS_SENSE_SIZE];

	/* request info for TIMEOUT event debug */
	struct request *rq;
	u64 deadline;
};

struct ufs_devcmd_info {
	u8 lun;
	u32 tag;
	u64 time_cost;
	int ocs;
	struct utp_upiu_req req;
	struct utp_upiu_rsp rsp;
};

struct ufs_uic_cmd_info {
	u32 cmd;
	u32 argu1;
	u32 argu2;
	u32 argu3;
	u32 upmcrs;
	bool pwr_change;
	/* True if caps UFS_SPRD_CAP_ACC_FORBIDDEN_AFTER_H8_EE is set */
	bool dwc_hc_ee_h8_compl;
};

struct ufs_tm_cmd_info {
	u8 tm_func;
	u32 lun;
	u32 tag;
	u32 param1;
	int ocs;
};

struct ufs_int_error {
	u32 errors;
	u32 uic_error;
};

struct ufs_clk_dbg {
	u32 status;
	u32 on;
};

struct ufs_evt_dbg {
	u32 id;
	u32 val;
};

struct ufs_event_info {
	enum ufs_event_list event;
	pid_t pid;
	u32 cpu;
	bool flag;
	bool panic_f;
	ktime_t time;
	u64 jiffies;
	union {
		struct ufs_cmd_info ci;
		struct ufs_uic_cmd_info uci;
		struct ufs_devcmd_info dmi;
		struct ufs_tm_cmd_info tmi;
		struct ufs_int_error ie;
		struct ufs_clk_dbg cd;
		struct ufs_evt_dbg evt;
	} pkg;
};

enum err_type {
	UFS_SPRD_RESET,
	UFS_LINE_RESET,
	UFS_SCSI_TIMEOUT,
};

struct ufs_err_cnt {
	unsigned long long sprd_reset_cnt;
	unsigned long long line_reset_cnt;
	unsigned long long scsi_timeout_cnt;
};

enum ufs_uic_err_code_event {
	/* uic error code PHY Adapter Layer */
	UFS_EVT_PA_PHY_LINE0_ERR = 0,
	UFS_EVT_PA_PHY_LINE1_ERR,
	UFS_EVT_PA_PHY_LINE2_ERR,
	UFS_EVT_PA_PHY_LINE3_ERR,
	UFS_EVT_PA_GRNERIC_PA_ERR,
	UIC_ERR_PA_MAX,

	/* uic error code Data Link Layer */
	UFS_EVT_DL_NAC_RECEIVED = 0,
	UFS_EVT_DL_TCx_REPLAY_TIMER_EXPIRED,
	UFS_EVT_DL_AFCx_REQUEST_TIMER_EXPIRED,
	UFS_EVT_DL_FCx_PROTECTION_TIMER_EXPIRED,
	UFS_EVT_DL_CRC_ERROR,
	UFS_EVT_DL_RX_BUFFER_OVERFLOW,
	UFS_EVT_DL_MAX_FRAME_LENGTH_EXCEEDED,
	UFS_EVT_DL_WRONG_SEQUENCE_NUMBER,
	UFS_EVT_DL_AFC_FRAME_SYNTAX_ERROR,
	UFS_EVT_DL_NAC_FRAME_SYNTAX_ERROR,
	UFS_EVT_DL_EOF_SYNTAX_ERROR,
	UFS_EVT_DL_FRAME_SYNTAX_ERROR,
	UFS_EVT_DL_BAD_CTRL_SYMBOL_TYPE,
	UFS_EVT_DL_PA_INIT_ERROR,
	UFS_EVT_DL_PA_ERROR_IND_RECEIVED,
	UFS_EVT_DL_PA_INIT,
	UIC_ERR_DL_MAX,

	/* uic error code Natwork Layer */
	UFS_EVT_NL_UNSUPPORTED_HEADER_TYPE = 0,
	UFS_EVT_NL_BAD_DEVICEID_ENC,
	UFS_EVT_NL_LHDR_TRAP_PACKET_DROPPING,
	UFS_EVT_NL_MAX_N_PDU_LENGTH_EXCEEDED,
	UIC_ERR_NL_MAX,

	/* uic error code Transport Layer */
	UFS_EVT_TL_UNSUPPORTED_HEADER_TYPE = 0,
	UFS_EVT_TL_UNKNOWN_CPORTID,
	UFS_EVT_TL_NO_CONNECTION_RX,
	UFS_EVT_TL_CONTROLLED_SEGMENT_DROPPING,
	UFS_EVT_TL_BAD_TC,
	UFS_EVT_TL_E2E_CREDIT_OVERFLOW,
	UFS_EVT_TL_SAFETY_VALVE_DROPPING,
	UFS_EVT_TL_MAX_T_PDU_LENGTH_EXCEEDED,
	UIC_ERR_TL_MAX,

	/* uic error code DME*/
	UFS_EVT_DME_GENERIC_ERR = 0,
	UFS_EVT_DME_QOS_FROM_TX_DETECTED,
	UFS_EVT_DME_QOS_FROM_RX_DETECTED,
	UFS_EVT_DME_QOS_FROM_PA_INIT_DETECTED,
	UIC_ERR_DME_MAX,
};

struct ufs_uic_err_code_cnt {
	unsigned long long pa_err_cnt[UIC_ERR_PA_MAX];
	unsigned long long dl_err_cnt[UIC_ERR_DL_MAX];
	unsigned long long nl_err_cnt[UIC_ERR_NL_MAX];
	unsigned long long tl_err_cnt[UIC_ERR_TL_MAX];
	unsigned long long dme_err_cnt[UIC_ERR_DME_MAX];
};

#define B_WB_AVAIL_BUF_ALL	0xa
#define SIZE_MB			(1024 * 1024)

#define UFS_WB_BUF_REMAIN_MB(avail, cur_buf, unit_size)		\
	((avail) * (cur_buf) * (unit_size) / B_WB_AVAIL_BUF_ALL / SIZE_MB)

#define FROM			3   /* 2 ^ FROM ms */
#define UFS_PERF_ARRAY_SIZE	12

struct ufs_throughput {
	u64	read_speed;	/* Read speed in Mbytes per second */
	u64	write_speed;	/* Write speed in Mbytes per second */
	u64	read_len;	/* Total bytes read during the measurement period */
	u64	write_len;	/* Total bytes written during the measurement period */
	ktime_t	read_time;	/* Total duration (in ktime_t) spent on read operations */
	ktime_t	write_time;	/* Total duration (in ktime_t) spent on write operations */
};

struct ufs_perf_data {
	u64			time;
	/*
	 * match with ms_to_index/io_log in bsp/modules/common/io/block/kprobe_block.c
	 * convert ms to index:
	 * --------------------------------------------
	 *     index	:	time
	 * --------------------------------------------
	 *	0	:	[   0	-    8ms)
	 *	1	:	[   8	-   16ms)
	 *	2	:	[  16	-   32ms)
	 *	3	:	[  32	-   64ms)
	 *	4	:	[  64	-  128ms)
	 *	5	:	[ 128	-  256ms)
	 *	6	:	[ 256	-  512ms)
	 *	7	:	[ 512	- 1024ms)
	 *	8	:	[1024	- 2048ms)
	 *	9	:	[2048	- 4096ms)
	 *	10	:	[4096	- 8192ms)
	 *	11	:	[8192	~
	 * --------------------------------------------
	 */
	u32			r_d2c[UFS_PERF_ARRAY_SIZE];
	u32			w_d2c[UFS_PERF_ARRAY_SIZE];
	/*
	 * convert ms to index:
	 * if UFS_PERF_ARRAY_SIZE=12, there is:
	 * --------------------------------------------
	 *    index	:	    time
	 * --------------------------------------------
	 *	0	:	[   0	-    1ms)
	 *	1	:	[   1	-    2ms)
	 *	2	:	[   2	-    4ms)
	 *	3	:	[   4	-    8ms)
	 *	4	:	[   8	-   16ms)
	 *	5	:	[  16	-   32ms)
	 *	6	:	[  32	-   64ms)
	 *	7	:	[  64	-  128ms)
	 *	8	:	[ 128	-  256ms)
	 *	9	:	[ 256	-  512ms)
	 *	10	:	[ 512	- 1024ms)
	 *	11	:	[1024	~
	 * --------------------------------------------
	 */
	u16			h8_enter2exit[UFS_PERF_ARRAY_SIZE];

	u32			wb_avail_buf;
	u32			wb_cur_buf;
	u64			wb_time;
	u16			unmap_cmd_nums;
	bool			wb_valid;

	struct ufs_throughput	tp;
};

struct ufs_perf_s {
	bool			max_exceed;
	bool			work_initialized;
	bool			wb_initialized;
	u32			idx;
	u32			read_idx;
	u64			start;
	unsigned long		alloc_unit_size;
	unsigned long		segment_size;
	unsigned long		wb_unit_size;
	ktime_t			h8_enter_compl_time_stamp;
	struct work_struct	perf_work;
	struct ufs_perf_data	d[UFS_PERF_RECORD_DEPTH];
};

static inline unsigned int ms_to_index_h8(unsigned int ms)
{
	return ms > 0 ? min((UFS_PERF_ARRAY_SIZE - 1), fls(ms)) : 0;
}

static inline unsigned int ms_to_index_rw(unsigned int ms)
{
	ms >>= FROM;
	return ms > 0 ? min((UFS_PERF_ARRAY_SIZE - 1), fls(ms)) : 0;
}

#define RW_LAT_MS_TO_IDX(lrbp)	\
	ms_to_index_rw(ktime_to_ms((lrbp)->compl_time_stamp - (lrbp)->issue_time_stamp))

#define H8_LAT_MS_TO_IDX(p)	\
	ms_to_index_h8(ktime_to_ms(ktime_get() - (p)->h8_enter_compl_time_stamp))

extern spinlock_t ufs_dbg_regs_lock;

int ufs_sprd_debug_init(struct ufs_hba *hba);
void ufshcd_common_trace(struct ufs_hba *hba, enum ufs_event_list event, void *data);
bool sprd_ufs_debug_is_supported(struct ufs_hba *hba);
void sprd_ufs_debug_err_dump(struct ufs_hba *hba);
void sprd_ufs_print_err_cnt(struct ufs_hba *hba);
void ufs_sprd_update_err_cnt(struct ufs_hba *hba, u32 reg, enum err_type type);
void ufs_sprd_update_uic_err_cnt(struct ufs_hba *hba, u32 reg, enum ufs_event_type evt);
void ufs_sprd_sysfs_add_nodes(struct ufs_hba *hba);
extern struct ufs_hba *hba_tmp;
void ufs_sprd_cmd_history_dump(u32 dump_req, struct seq_file *m, char **dump_pos);

void *ufs_sprd_get_dbg_hist(void);
void ufs_sprd_dbg_regs_hist_register(struct ufs_hba *hba, u32 regs_num, void *name_ptr);

void ufs_sprd_perf_trace(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void ufs_sprd_perf_work_init(struct ufs_hba *hba);
void ufs_sprd_uic_cmd_record(struct ufs_hba *hba, struct uic_command *ucmd, int str);
#endif/* _UFS_SPRD_DEBUG_H_ */

