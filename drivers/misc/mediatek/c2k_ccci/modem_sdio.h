/*
 *drivers/mmc/card/modem_sdio.h
 *
 *VIA CBP SDIO driver for Linux
 *
 *Copyright (C) 2009 VIA TELECOM Corporation, Inc.
 *Author: VIA TELECOM Corporation, Inc.
 *
 *This package is free software; you can redistribute it and/or modify
 *it under the terms of the GNU General Public License version 2 as
 *published by the Free Software Foundation.
 *
 *THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef MODEM_SDIO_H
#define MODEM_SDIO_H

#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/kfifo.h>
#include <mt-plat/mt_ccci_common.h>
#include "cbp_sdio.h"

#define ENABLE_CCMNI	(1)
#define ENABLE_CHAR_DEV (1)

#define SDIO_TTY_CHAN_ID_BEGIN	1
#define SDIO_TTY_CHAN_ID_END	13
#define SDIO_TTY_NR		20	/*Number of UARTs this driver can handle */

#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
#define CTRL_CH_ID			0
#define DATA_CH_ID			1
#define MD_LOG_CH_ID		2
#define FLS_CH_ID			3	/*flashlessd channel id */
#define SDIO_AT_CHANNEL_NUM		(4)
#define AGPS_CH_ID			6
#define SDIO_AT2_CHANNEL_NUM	(7)
#define SDIO_AT3_CHANNEL_NUM	(8)
#define EXCP_CTRL_CH_ID		11
#define EXCP_MSG_CH_ID		12
#define EXCP_DATA_CH_ID		13
#else
#define CTRL_CH_ID			0
#define DATA_CH_ID			2
#define MD_LOG_CH_ID		3
#define FLS_CH_ID			4	/*flashlessd channel id */
#define SDIO_AT_CHANNEL_NUM		(5)
#define AGPS_CH_ID			6
#define SDIO_AT2_CHANNEL_NUM	(7)
#define SDIO_AT3_CHANNEL_NUM	(8)
#define MD_LOG2_CH_ID		10
#define EXCP_CTRL_CH_ID		11
#define EXCP_MSG_CH_ID		12
#define EXCP_DATA_CH_ID		13
#define CCMNI_AP_LOOPBACK_CH	(15)
#define SDIO_AT4_CHANNEL_NUM	(16)
#define SDIO_AT5_CHANNEL_NUM	(17)
#define SDIO_AT6_CHANNEL_NUM	(18)
#define SDIO_AT7_CHANNEL_NUM	(19)
#define SDIO_AT8_CHANNEL_NUM	(20)
#endif

#define HEART_BEAT_TIMEOUT		(10000)	/*ms */
#define POLLING_INTERVAL		(15000)	/*ms */
#define FORCE_ASSERT_TIMEOUT	(5000)	/*ms */

/*
*SDIO buffer-in lens.
*when tty port is not opened by upper layer, we buffer some data from md, when opened, push to tty.
*BUF_IN_MAX_NUM means the max packet number we will buffer for each service.
*/
#define SDIO_PPP_BUF_IN_MAX_NUM		100
#define SDIO_ETS_BUF_IN_MAX_NUM		500
#define SDIO_IFS_BUF_IN_MAX_NUM		100
#define SDIO_AT_BUF_IN_MAX_NUM		100
#define SDIO_PCV_BUF_IN_MAX_NUM		100
#define SDIO_DEF_BUF_IN_MAX_NUM		100
#if ENABLE_CHAR_DEV
#define SDIO_BUF_IN_MAX_SIZE		(256*1024)
#else
#define SDIO_BUF_IN_MAX_SIZE		64512	/*max buffer-in size = 63K, for each port */
#endif

#define MODEM_FC_PRINT_MAX			3	/*for print flow control logs */
#define SDIO_FUNC_1_IRQ				(1<<1)	/*for SDIO irq source check */

#define SDIO_BUF_MAX				4096
#define SDIO_ASSEMBLE_MAX			(100*1024)

#define ONE_PACKET_MAX_SIZE			(5200)

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
#define SDIO_WRITE_ADDR				SDIO_CTDR
#else
#define SDIO_WRITE_ADDR				0x00
#endif

#define MSG_START_FLAG				0xFE
#define SDIO_MSG_MAX_LEN			4096

#define MORE_DATA_FOLLOWING				(0x20)
#define EXTEND_CH_BIT					(0x10)
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
struct sdio_hw_head {
	unsigned char len_low;
	unsigned char len_hi;
	unsigned char reserved1;
	unsigned char reserved2;
};
#endif
struct sdio_msg_head {
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	struct sdio_hw_head hw_head;
#endif
	unsigned char start_flag;	/*start flag(0xFE), little endian */
	unsigned char chanInfo;	/*channel id */
	unsigned char tranHi;	/*len High byte, little endian */
	unsigned char tranLow;	/*len low byte, little endian */
};

struct sdio_msg {
	struct sdio_msg_head head;
	unsigned char buffer[SDIO_MSG_MAX_LEN];
};

enum sflow_ctrl_state {
	SFLOW_CTRL_DISABLE = 0,	/*default disable */
	SFLOW_CTRL_ENABLE	/*when md request, we set this */
};

enum ccmni_tx_state {
	CCMNI_TX_READY = 0,	/*default is ready */
	CCMNI_TX_STOP,		/*when tx busy, we set this */
};

struct sdio_modem_port {
	struct sdio_modem *modem;	/*all port use one sdio_modem */
	struct sdio_func *func;
#if ENABLE_CHAR_DEV
	struct cdev char_dev;
	atomic_t usage_cnt;
	wait_queue_head_t rx_wq;
#else
	struct tty_port port;
	struct tty_struct *tty;
#endif
	struct kref kref;
	spinlock_t write_lock;
	unsigned int index;

	struct kfifo transmit_fifo;
	const char *name;
	char work_name[64];
	struct workqueue_struct *write_q;
	struct work_struct write_work;
#if ENABLE_CCMNI
	struct work_struct write_ccmni_work;
	enum ccmni_tx_state tx_state;
	spinlock_t tx_state_lock;
#endif
	struct device dev;

	struct mutex sdio_buf_in_mutex;
	struct list_head sdio_buf_in_list;
	unsigned char sdio_buf_in;
	unsigned int sdio_buf_in_num;	/*buffer in list num */
	unsigned int sdio_buf_in_max_num;	/*buffer in list max num */
	unsigned int sdio_buf_in_size;	/*buffer in size */

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	struct mutex sdio_assemble_mutex;
	struct list_head sdio_assemble_list;
#endif

	wait_queue_head_t sflow_ctrl_wait_q;
	atomic_t sflow_ctrl_state;
	struct semaphore write_sem;

	unsigned int tx_count;
	unsigned int rx_count;

	/*Settings for the port */
	int rts_state;		/*Handshaking pins (outputs) */
	int dtr_state;
	int cts_state;		/*Handshaking pins (inputs) */
	int dsr_state;
	spinlock_t inception_lock;
	int inception;
};

struct sdio_buf_in_packet {
	struct list_head node;
	unsigned int size;
#if ENABLE_CHAR_DEV
	unsigned int offset;
	unsigned int o_size;
#endif
	unsigned char *buffer;
};

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
struct sdio_assemble_packet {
	struct list_head node;
	unsigned int size;
	unsigned char *buffer;
	atomic_t occupied;
};
#endif

struct ctrl_port_msg {
	struct sdio_msg_head head;
	unsigned char id_hi;
	unsigned char id_low;
	unsigned char chan_num;
	unsigned char option;
};

struct sdio_modem_ctrl_port {
	struct ctrl_port_msg chan_ctrl_msg;
	unsigned int chan_state;
	unsigned int msg_id;
	wait_queue_head_t sflow_ctrl_wait_q;
	atomic_t sflow_ctrl_state;
};

struct _ccci_msg {
	union {
		u32 magic;	/*For mail box magic number */
		u32 addr;	/*For stream start addr */
		u32 data0;	/*For ccci common data[0] */
	};
	union {
		u32 id;		/*For mail box message id */
		u32 len;	/*For stream len */
		u32 data1;	/*For ccci common data[1] */
	};
	u32 channel;
	u32 reserved;
} __packed;

#define CCCI_CONTROL_TX_CH		1
#define EXCP_MSG_MAX_LEN		(SDIO_MSG_MAX_LEN - sizeof(struct _ccci_msg))

struct _exception_msg {
	struct _ccci_msg ccci_head;
	char buffer[EXCP_MSG_MAX_LEN];
};

/*MODEM MAUI Exception header (4 bytes)*/
struct _exception_record_header_t {
	u8 ex_type;
	u8 ex_nvram;
	u16 ex_serial_num;
} __packed;

/*MODEM MAUI Environment information (164 bytes) */
struct _ex_environment_info_t {
	u8 boot_mode;
	u8 reserved1[8];
	u8 execution_unit[8];
	u8 reserved2[147];
} __packed;

/*MODEM MAUI Special for fatal error (8 bytes)*/
struct _ex_fatalerror_code_t {
	u32 code1;
	u32 code2;
} __packed;

/*MODEM MAUI fatal error (296 bytes)*/
struct _ex_fatalerror_t {
	struct _ex_fatalerror_code_t error_code;
	u8 reserved1[288];
} __packed;

/*MODEM MAUI Assert fail (296 bytes)*/
struct _ex_assert_fail_t {
	u8 filename[24];
	u32 linenumber;
	u32 parameters[3];
	u8 reserved1[256];
} __packed;

/*MODEM MAUI Globally exported data structure (300 bytes) */
union ex_content_u {
	struct _ex_fatalerror_t fatalerr;
	struct _ex_assert_fail_t assert;
} __packed;

/*MODEM MAUI Standard structure of an exception log ( */
struct _ex_exception_log_t {
	struct _exception_record_header_t header;
	u8 reserved1[12];
	struct _ex_environment_info_t envinfo;
	u8 reserved2[36];
	union ex_content_u content;
} __packed;

struct dump_debug_info {
	unsigned int type;
	char *name;
	unsigned int more_info;
	union {
		struct {
			char file_name[30];
			int line_num;
			unsigned int parameters[3];
		} assert;
		struct {
			int err_code1;
			int err_code2;
			char offender[9];
		} fatal_error;
		struct _ccci_msg data;
		struct {
			unsigned char execution_unit[9];	/*8+1 */
			char file_name[30];
			int line_num;
			unsigned int parameters[3];
		} dsp_assert;
		struct {
			unsigned char execution_unit[9];
			unsigned int code1;
		} dsp_exception;
		struct {
			unsigned char execution_unit[9];
			unsigned int err_code[2];
		} dsp_fatal_err;
	};
	void *ext_mem;
	size_t ext_size;
	void *md_image;
	size_t md_size;
	void *platform_data;
	void (*platform_call)(void *data);
};

enum {
	C2K_MD_EX = 0x4,
	C2K_MD_EX_REC_OK = 0x6,
	C2K_MD_EX_CHK_ID = 0x45584350,
	C2K_MD_EX_REC_OK_CHK_ID = 0x45524543,
};

enum {
	MD_EX_TYPE_INVALID = 0,
	MD_EX_TYPE_UNDEF = 1,
	MD_EX_TYPE_SWI = 2,
	MD_EX_TYPE_PREF_ABT = 3,
	MD_EX_TYPE_DATA_ABT = 4,
	MD_EX_TYPE_ASSERT = 5,
	MD_EX_TYPE_FATALERR_TASK = 6,
	MD_EX_TYPE_FATALERR_BUF = 7,
	MD_EX_TYPE_LOCKUP = 8,
	MD_EX_TYPE_ASSERT_DUMP = 9,
	MD_EX_TYPE_ASSERT_FAIL = 10,
	DSP_EX_TYPE_ASSERT = 11,
	DSP_EX_TYPE_EXCEPTION = 12,
	DSP_EX_FATAL_ERROR = 13,
	STACKACCESS_EXCEPTION = 14,

	/*cross core trigger exception */
	CC_MD1_EXCEPTION = 15,
	NUM_EXCEPTION,

	MD_EX_TYPE_EMI_CHECK = 99,
	MD_EX_LTE_FATAL_ERROR = 0x3000,
};

#define EE_BUF_LEN		(256)
#define AED_STR_LEN		(512)

#define CCCI_EXREC_OFFSET_OFFENDER 288

/*=================================================================================*/
/*Image type and header defination part*/
/*=================================================================================*/

#define C2K_IMG_PATH	"/etc/firmware/modem_3_3g_n.img"

enum {
	MD_BOOTING = 0,
	MD_READY,
	MD_EXCEPTION,
	MD_EXCEPTION_ONGOING,
	MD_RESET_ON_GOING,
	MD_OFF,
};

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
union sdio_pio_int_sts_reg {
	struct {
		unsigned int fw_own_back:1;
		unsigned int rx_rdy:1;
		unsigned int tx_empty:1;
		unsigned int tx_under_thold:1;
		unsigned int tx_cmpl_cnt:3;
		unsigned int fw_int_indicator:1;
		unsigned int tx_overflow:1;
		unsigned int fw_int:7;
		unsigned int rx_pkt_len:16;
	} u;
	unsigned int raw_val;
};

union sdio_pio_int_mask_reg {
	struct {
		unsigned int fw_own_back_en:1;
		unsigned int rx_rdy_en:1;
		unsigned int tx_empty_en:1;
		unsigned int tx_under_thold_en:1;
		unsigned int resv0:3;
		unsigned int fw_int_indicator_en:1;
		unsigned int tx_overflow_en:1;
		unsigned int fw_int:7;
		unsigned int resv1:16;
	} u;
	unsigned int raw_val;
};

#define DEFAULT_BLK_SIZE	512
#define RX_FIFO_SZ			2304	/*?? need to confirm. Haow */
#define TX_FIFO_SZ			2080
#define DEFAULT_TX_THOLD	4

#define LOG_MD_DATA_READY_MSG 0xA0
#define LOG_AP_DATA_DONE_MSG 0xA1
struct smem_log_block {
	unsigned short id;
	unsigned int address;
	unsigned int length;
	unsigned short reserved[3];
} __packed;
#endif

struct sdio_modem {
	struct sdio_modem_port *port[SDIO_TTY_NR];
	struct sdio_modem_ctrl_port *ctrl_port;
	struct sdio_func *func;
	struct sdio_msg *msg;
	unsigned char *trans_buffer;
	struct cbp_platform_data *cbp_data;
	struct semaphore sem;
	unsigned int data_length;

	unsigned int status;
	struct dump_debug_info debug_info;
	struct _ex_exception_log_t ex_info;

	spinlock_t status_lock;

	/*statistic */
	unsigned int tx_count;
	unsigned int rx_count;

	/*for USB rawbulk */
	struct work_struct dtr_work;
	struct work_struct dcd_query_work;

	struct kobject *c2k_kobj;
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	/*hw related settings */
	int fw_own;
	union sdio_pio_int_sts_reg int_sts;
	union sdio_pio_int_mask_reg int_mask;
	int int_clr_ctl;

	struct sdio_assemble_packet *as_packet;
	struct work_struct loopback_work;

	atomic_t tx_fifo_cnt;
	wait_queue_head_t wait_tx_done_q;

	/*because we use timer to send heart beat msg and force assert msg, so workqueue is needed. */
	struct work_struct poll_hb_work;	/*used to send heart beat msg to c2k */
	struct work_struct force_assert_work;	/*used to send force assert msg to c2k */
	struct work_struct smem_read_done_work;	/*used to send smem-log-read-done msg to C2K */
	struct smem_log_block curr_log_blk;
	unsigned long long log_blk_stamp;

	struct timer_list heart_beat_timer;	/*when this timer expired, we assume c2k md is dead, trigger EE */
	struct timer_list poll_timer;	/*this timer is used to poll md's status every certain seconds */
	struct timer_list force_assert_timer;	/*this timer is used to poll md's status every certain seconds */

	struct ccci_image_info img_info;
#endif
};
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
enum sdio_int_clr_ctl {
	SDIO_INT_CTL_RC = 0,
	SDIO_INT_CTL_W1C,
};
#endif
extern int sdio_log_level;

#define LOG_ERR			0
#define LOG_INFO		1
#define LOG_NOTICE		2
#define LOG_NOTICE2		3
#define LOG_DEBUG		4

#define LOGPRT(lvl, x...)  do { \
	if (lvl < (sdio_log_level + 1)) \
		pr_err("[C2K MODEM] " x); \
	} while (0)

extern struct ccmni_dev_ops ccmni_ops;

extern int modem_sdio_init(struct cbp_platform_data *pdata);
extern void modem_sdio_exit(void);
extern int modem_err_indication_usr(int revocery);
extern int modem_ipoh_indication_usr(void);

extern void dump_c2k_iram(void);
extern void dump_c2k_iram_seg2(void);

extern unsigned int get_c2k_reserve_mem_size(void);
extern char *get_ap_platform(void);

int sdio_modem_ccmni_send_pkt(int md_id, int ccmni_idx, void *data, int is_ack);
/*
int sdio_modem_get_ccmni_ch(int md_id, int ccmni_idx, struct ccmni_ch *channel);
*/
extern void msdc_c2k_dump_int_register(void);

/*use following function to push data to USB, for ETS*/
extern int rawbulk_push_upstream_buffer(int transfer_id, const void *buffer,
					unsigned int length);
/*use following function when modem attach success*/
extern int rawbulk_bind_sdio_channel(int transfer_id);
/*use following function when modem removed, after this, ETS will not receive any log*/
extern void rawbulk_unbind_sdio_channel(int transfer_id);

/*used by USB for ETS*/
void modem_dtr_send(struct work_struct *work);

#define USE_CCIF_INTR			(0)

#if USE_CCIF_INTR
extern int ccif_notify_c2k(int ch_id);
extern int dump_ccif(void);
#endif

extern phys_addr_t md3_mem_base;

extern void mt_irq_dump_status(int irq);
extern void mt_eint_dump_status(unsigned int eint);
extern int c2k_modem_not_ready(void);
extern void dump_c2k_bootup_status(void);

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
extern void c2k_sdio_install_eirq(void);
extern void c2k_sdio_uninstall_eirq(void);
#endif

extern void SRC_trigger_signal(int i_on);

#endif
