/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccci_md.h
 *
 * Project:
 * --------
 *   Andes
 *
 * Description:
 * ------------
 *   MT65XX Modem initialization and handshake header file
 *
 ****************************************************************************/

#ifndef __CCCI_MD_H__
#define __CCCI_MD_H__
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#define CCCI_SYSFS_MD_INIT "modem"
#define CCCI_SYSFS_MD_BOOT_ATTR "boot"
#define MD_BOOT_CMD_CHAR '0'
#define NORMAL_BOOT_ID 0
#define META_BOOT_ID 1
/* #define MD_RUNTIME_ADDR (CCIF_BASE + 0x0140) */
/* #define SLEEP_CON 0xF0001204 */
/* #define CCCI_CURRENT_VERSION 0x00000923 */
#define NR_CCCI_RESET_USER 10
#define NR_CCCI_RESET_USER_NAME 16

#define CCCI_UART_PORT_NUM 8

#define CCCI_MD_EXCEPTION   0x1
#define CCCI_MD_RESET     0x2
#define CCCI_MD_BOOTUP    0x3
#define CCCI_MD_STOP      0x4

#define LOCK_MD_SLP        0x1
#define UNLOCK_MD_SLP        0x0

#define MD_IMG_MAX_CNT    0x4
/*-----------------------------------------------------------*/
/* Device ID assignment */
#define CCCI_TTY_DEV_MAJOR        (169)	/* (0: Modem; 1: Meta; 2:IPC) */

enum {
	MD_BOOT_STAGE_0 = 0,
	MD_BOOT_STAGE_1 = 1,
	MD_BOOT_STAGE_2 = 2,
	MD_BOOT_STAGE_EXCEPTION = 3
};

enum {
	MD_INIT_START_BOOT = 0x00000000,
	MD_INIT_CHK_ID = 0x5555FFFF,
	MD_EX = 0x00000004,
	MD_EX_CHK_ID = 0x45584350,
	MD_EX_REC_OK = 0x00000006,
	MD_EX_REC_OK_CHK_ID = 0x45524543,
	MD_EX_RESUME_CHK_ID = 0x7,
	CCCI_DRV_VER_ERROR = 0x5,

	/*  System channel, AP->MD || AP<-->MD message start from 0x100 */
/*    MD_DORMANT_NOTIFY = 0x100,
    MD_SLP_REQUEST = 0x101,
    MD_TX_POWER = 0x102,
    MD_RF_TEMPERATURE = 0x103,
    MD_RF_TEMPERATURE_3G = 0x104,
    MD_GET_BATTERY_INFO = 0x105,
*/
	/*  System channel, MD --> AP message start from 0x1000 */
	MD_WDT_MONITOR = 0x1000,
	MD_WAKEN_UP = 0x10000,
};

enum {
	ER_MB_START_CMD = -1,
	ER_MB_CHK_ID = -2,
	ER_MB_BOOT_READY = -3,
	ER_MB_UNKNOWN_STAGE = -4
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
	NUM_EXCEPTION
};
#define MD_EX_TYPE_EMI_CHECK 99

enum {
	MD_EE_FLOW_START = 0,
	MD_EE_DUMP_ON_GOING,
	MD_STATE_UPDATE,
	MD_EE_MSG_GET,
	MD_EE_TIME_OUT_SET,
	MD_EE_OK_MSG_GET,
	MD_EE_FOUND_BY_ISR,
	MD_EE_FOUND_BY_TX,
	MD_EE_PENDING_TOO_LONG,

	MD_EE_INFO_OFFSET = 20,
	MD_EE_EXCP_OCCUR = 20,
	MD_EE_AP_MASK_I_BIT_TOO_LONG = 21,
};

enum {
	MD_EE_CASE_NORMAL = 0,
	MD_EE_CASE_ONLY_EX,
	MD_EE_CASE_ONLY_EX_OK,
	MD_EE_CASE_TX_TRG,
	MD_EE_CASE_ISR_TRG,
	MD_EE_CASE_NO_RESPONSE,
	MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG,
};

#ifdef AP_MD_EINT_SHARE_DATA
enum {
	CCCI_EXCH_CORE_AWAKEN = 0,
	CCCI_EXCH_CORE_SLEEP = 1,
	CCCI_EXCH_CORE_SLUMBER = 2
};
#endif

/* CCCI system message */
enum {
	CCCI_SYS_MSG_RESET_MD = 0x20100406
};

/* MD Message, this is for user space daemon use */
enum {
	CCCI_MD_MSG_BOOT_READY = 0xFAF50001,
	CCCI_MD_MSG_BOOT_UP = 0xFAF50002,
	CCCI_MD_MSG_EXCEPTION = 0xFAF50003,
	CCCI_MD_MSG_RESET = 0xFAF50004,
	CCCI_MD_MSG_RESET_RETRY = 0xFAF50005,
	CCCI_MD_MSG_READY_TO_RESET = 0xFAF50006,
	CCCI_MD_MSG_BOOT_TIMEOUT = 0xFAF50007,
	CCCI_MD_MSG_STOP_MD_REQUEST = 0xFAF50008,
	CCCI_MD_MSG_START_MD_REQUEST = 0xFAF50009,
	CCCI_MD_MSG_ENTER_FLIGHT_MODE = 0xFAF5000A,
	CCCI_MD_MSG_LEAVE_FLIGHT_MODE = 0xFAF5000B,
	CCCI_MD_MSG_POWER_ON_REQUEST = 0xFAF5000C,
	CCCI_MD_MSG_POWER_DOWN_REQUEST = 0xFAF5000D,
	CCCI_MD_MSG_SEND_BATTERY_INFO = 0xFAF5000E,
	CCCI_MD_MSG_NOTIFY = 0xFAF5000F,
	CCCI_MD_MSG_STORE_NVRAM_MD_TYPE = 0xFAF50010,
	CCCI_MD_MSG_CFG_UPDATE = 0xFAF50011,
};

/* MD Status, this is for user space daemon use */
enum {
	CCCI_MD_STA_BOOT_READY = 0,
	CCCI_MD_STA_BOOT_UP = 1,
	CCCI_MD_STA_RESET = 2,
};

#if 0
/* MODEM MAUI SW ASSERT LOG */
struct modem_assert_log {
	char ex_type;
	char ex_nvram;
	short ex_serial;
	char data1[212];
	char filename[24];
	int linenumber;
	char data2[268];
};

/* MODEM MAUI SW FATAL ERROR LOG */
struct modem_fatalerr_log {
	char ex_type;
	char ex_nvram;
	short ex_serial;
	char data1[212];
	int err_code1;
	int err_code2;
	char data2[288];
};
#endif

struct cores_sleep_info {
	unsigned char AP_Sleep;
	unsigned char padding1[3];
	unsigned int RTC_AP_WakeUp;
	unsigned int AP_SettleTime;	/* clock settle duration */
	unsigned char MD_Sleep;
	unsigned char padding2[3];
	unsigned int RTC_MD_WakeUp;
	unsigned int RTC_MD_Settle_OK;	/* clock settle done time */
};

/* MODEM MAUI Exception header (4 bytes)*/
struct EX_HEADER_T {
	unsigned char ex_type;
	unsigned char ex_nvram;
	unsigned short ex_serial_num;
};

/* MODEM MAUI Environment information (164 bytes) */
struct EX_ENVINFO_T {
	unsigned char boot_mode;
	unsigned char reserved1[8];
	unsigned char execution_unit[8];
	unsigned char reserved2[147];
};

/* MODEM MAUI Special for fatal error (8 bytes)*/
struct EX_FATALERR_CODE_T {
	unsigned int code1;
	unsigned int code2;
};

/* MODEM MAUI fatal error (296 bytes)*/
struct EX_FATALERR_T {
	struct EX_FATALERR_CODE_T error_code;
	unsigned char reserved1[288];
};

/* MODEM MAUI Assert fail (296 bytes)*/
struct EX_ASSERTFAIL_T {
	unsigned char filename[24];
	unsigned int linenumber;
	unsigned int parameters[3];
	unsigned char reserved1[256];
};

/* MODEM MAUI Globally exported data structure (300 bytes) */
union EX_CONTENT_T {
	struct EX_FATALERR_T fatalerr;
	struct EX_ASSERTFAIL_T assert;
};

/* MODEM MAUI Standard structure of an exception log ( */
struct EX_LOG_T {
	struct EX_HEADER_T  header;
	unsigned char reserved1[12];
	struct EX_ENVINFO_T envinfo;
	unsigned char reserved2[36];
	union EX_CONTENT_T content;
};

struct core_eint_config {
	unsigned char eint_no;
	unsigned char Sensitivity;
	unsigned char ACT_Polarity;
	unsigned char Dbounce_En;
	unsigned int Dbounce_ms;
};

struct ccci_cores_exch_data {
	struct cores_sleep_info sleep_info;
	unsigned int report_os_tick;	/* report OS Tick Periodic in second unit */
	/* ( 0 = disable ) */
	unsigned int nr_eint_config;
	unsigned int eint_config_offset;	/* offset from SysShareMemBase for struct coreeint_config */
};

#define CCCI_SYS_SMEM_SIZE sizeof(struct ccci_cores_exch_data)

struct ccci_reset_sta {
	int is_allocate;
	int is_reset;
	char name[NR_CCCI_RESET_USER_NAME];
};

struct modem_runtime_t {
	int Prefix;		/*  "CCIF" */

	int Platform_L;		/*  Hardware Platform String ex: "MT6589E1" */
	int Platform_H;
	int DriverVersion;	/*  0x20121001 since W12.39 */
	int BootChannel;	/*  Channel to ACK AP with boot ready */
	int BootingStartID;	/*  MD is booting. NORMAL_BOOT_ID or META_BOOT_ID  */
	int BootAttributes;	/*  Attributes passing from AP to MD Booting */
	int BootReadyID;	/*  MD response ID if boot successful and ready */
	int MdlogShareMemBase;
	int MdlogShareMemSize;
	int PcmShareMemBase;
	int PcmShareMemSize;
	int UartPortNum;
	int UartShareMemBase[CCCI_UART_PORT_NUM];
	int UartShareMemSize[CCCI_UART_PORT_NUM];
	int FileShareMemBase;
	int FileShareMemSize;
	int RpcShareMemBase;
	int RpcShareMemSize;
	int PmicShareMemBase;
	int PmicShareMemSize;
	int ExceShareMemBase;
	int ExceShareMemSize;	/*  512 Bytes Required  */
	int SysShareMemBase;
	int SysShareMemSize;
	int IPCShareMemBase;
	int IPCShareMemSize;
	int MDULNetShareMemBase;
	int MDULNetShareMemSize;
	int MDDLNetShareMemBase;
	int MDDLNetShareMemSize;
	int NetPortNum;
	int NetULCtrlShareMemBase[NET_PORT_NUM];	/*  <<< Current NET_PORT_NUM is 4 */
	int NetULCtrlShareMemSize[NET_PORT_NUM];
	int NetDLCtrlShareMemBase[NET_PORT_NUM];
	int NetDLCtrlShareMemSize[NET_PORT_NUM];
	int MDExExpInfoBase;	/* md exception expand info memory */
	int MDExExpInfoSize;
	int IPCMDIlmShareMemBase;
	int IPCMDIlmShareMemSize;
	int MiscInfoBase;
	int MiscInfoSize;

	int CheckSum;
	int Postfix;		/* "CCIF"  */
};

#define CCCI_MD_RUNTIME_DATA_SMEM_SIZE (sizeof(struct modem_runtime_t))

struct modem_runtime_info_tag_t {
	int prefix;		/* "CCIF" */
	int platform_L;		/* Hardware platform string. ex: 'TK6516E0' */
	int platform_H;
	int driver_version;	/* 0x00000923 since W09.23 */
	int runtime_data_base;
	int runtime_data_size;
	int postfix;		/* "CCIF" */
};

struct modem_exception_exp_t {
	int exception_occur;
	int send_time;
	int wait_time;
};

struct MD_CALL_BACK_QUEUE {
	void (*call)(struct MD_CALL_BACK_QUEUE *, unsigned long data);
	struct MD_CALL_BACK_QUEUE *next;
};

struct MD_CALL_BACK_HEAD_T {
	spinlock_t lock;
	struct MD_CALL_BACK_QUEUE *next;
	int is_busy;
	struct tasklet_struct tasklet;
};

typedef int (*ccci_cores_sleep_info_base_req) (void *);
typedef int (*ccci_core_eint_config_setup) (int, void *);

int __init ccci_md_init_mod_init(void);
void __exit ccci_md_init_mod_exit(void);

int ccci_mdlog_base_req(int md_id, void *addr_vir, void *addr_phy,
			unsigned int *len);
int ccci_pcm_base_req(int md_id, void *addr_vir, void *addr_phy,
		      unsigned int *len);
int ccci_uart_base_req(int md_id, int port, void *addr_vir, void *addr_phy,
		       unsigned int *len);
int ccci_fs_base_req(int md_id, void *addr_vir, void *addr_phy,
		     unsigned int *len);
int ccci_rpc_base_req(int md_id, int *addr_vir, int *addr_phy, int *len);
int ccci_pmic_base_req(int md_id, void *addr_vir, void *addr_phy, int *len);
int ccci_ipc_base_req(int md_id, void *addr_vir, void *addr_phy, int *len);
int ccmni_v2_ul_base_req(int md_id, void *addr_vir, void *addr_phy);
int ccmni_v2_dl_base_req(int md_id, void *addr_vir, void *addr_phy);
int ccci_ccmni_v2_ctl_mem_base_req(int md_id, int port, int *addr_virt,
				   int *addr_phy, int *len);

int md_register_call_chain(int md_id, struct MD_CALL_BACK_QUEUE *queue);
int md_unregister_call_chain(int md_id, struct MD_CALL_BACK_QUEUE *queue);
void md_call_chain(struct MD_CALL_BACK_HEAD_T *head, unsigned long data);

int ccci_reset_register(int md_id, char *name);
int ccci_user_ready_to_reset(int md_id, int handle);

int get_curr_md_state(int md_id);
void check_data_connected(int md_id, int channel);

/* extern int ccci_sys_smem_base_phy; */
extern int ccci_smem_size;
extern int *ccci_smem_virt;
extern dma_addr_t ccci_smem_phy;
extern int is_first_boot;
extern struct MD_CALL_BACK_HEAD_T md_notifier;

#endif				/*  __CCCI_MD_H__ */
