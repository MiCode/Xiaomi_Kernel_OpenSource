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

#ifndef __CCCI_CORE_H__
#define __CCCI_CORE_H__

#include <linux/wait.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <mt-plat/mt_ccci_common.h>
#include "ccci_config.h"
#include "ccci_debug.h"
#include "ccci_fsm.h"

#define CCCI_MAGIC_NUM 0xFFFFFFFF
#define MAX_TXQ_NUM 8
#define MAX_RXQ_NUM 8
#define PACKET_HISTORY_DEPTH 16	/* must be power of 2 */

#define C2K_MD_LOG_TX_Q		3
#define C2K_MD_LOG_RX_Q		3
#define C2K_PCM_TX_Q		1
#define C2K_PCM_RX_Q		1

#define EX_TIMER_SWINT 10
#define EX_TIMER_MD_EX 5
#define EX_TIMER_MD_EX_REC_OK 10
#define EX_EE_WHOLE_TIMEOUT  (EX_TIMER_SWINT + EX_TIMER_MD_EX + EX_TIMER_MD_EX_REC_OK + 2) /* 2s is buffer */

#define CCCI_BUF_MAGIC 0xFECDBA89

/*
  * this is a trick for port->minor, which is configured in-sequence by different type (char, net, ipc),
  * but when we use it in code, we need it's unique among all ports for addressing.
  */
#define CCCI_IPC_MINOR_BASE 100
#define CCCI_NET_MINOR_BASE 200

struct ccci_log {
	struct ccci_header msg;
	u64 tv;
	int droped;
};

enum ccci_ipi_op_id {
	CCCI_OP_SCP_STATE,
	CCCI_OP_MD_STATE,
	CCCI_OP_SHM_INIT,
	CCCI_OP_SHM_RESET,

	CCCI_OP_LOG_LEVEL,
	CCCI_OP_GPIO_TEST,
	CCCI_OP_EINT_TEST,
	CCCI_OP_MSGSND_TEST,
	CCCI_OP_ASSERT_TEST,
};

enum {
	SCP_CCCI_STATE_INVALID = 0,
	SCP_CCCI_STATE_BOOTING = 1,
	SCP_CCCI_STATE_RBREADY = 2,
};

struct ccci_ipi_msg {
	u16 md_id;
	u16 op_id;
	u32 data[1];
} __packed;

/* enumerations and marcos */
typedef enum {
	EX_NONE = 0,
	EX_INIT,
	EX_DHL_DL_RDY,
	EX_INIT_DONE,
	/* internal use */
	MD_NO_RESPONSE,
	MD_WDT,
	MD_BOOT_TIMEOUT,
} MD_EX_STAGE;


typedef enum {
	MD_FLIGHT_MODE_NONE = 0,
	MD_FLIGHT_MODE_ENTER = 1,
	MD_FLIGHT_MODE_LEAVE = 2
} FLIGHT_STAGE;		/* for other module */

typedef struct _ccci_msg {
	union {
		u32 magic;	/* For mail box magic number */
		u32 addr;	/* For stream start addr */
		u32 data0;	/* For ccci common data[0] */
	};
	union {
		u32 id;		/* For mail box message id */
		u32 len;	/* For stream len */
		u32 data1;	/* For ccci common data[1] */
	};
	u32 channel;
	u32 reserved;
} __packed ccci_msg_t;


typedef enum {
	IDLE = 0,		/* update by buffer manager */
	FLYING,			/* update by buffer manager */
	PARTIAL_READ,		/* update by port_char */
} REQ_STATE;

typedef enum {
	IN = 0,
	OUT
} DIRECTION;

/*
 * This tells request free routine how it handles skb.
 * The CCCI request structure will always be recycled, but its skb can have different policy.
 * CCCI request can work as just a wrapper, due to netowork subsys will handler skb itself.
 * Tx: policy is determined by sender;
 * Rx: policy is determined by receiver;
 */
typedef enum {
	FREE = 0,		/* simply free the skb */
	RECYCLE,		/* put the skb back into our pool */
} DATA_POLICY;

/* ccci buffer control structure. Must be less than NET_SKB_PAD */
struct ccci_buffer_ctrl {
	unsigned int head_magic;
	DATA_POLICY policy;
	unsigned char ioc_override;	/* bit7: override or not; bit0: IOC setting */
	unsigned char blocking;
};

struct ccci_modem;
struct ccci_port;

struct ccci_modem_cfg {
	unsigned int load_type;
	unsigned int load_type_saving;
	unsigned int setting;
};
#define MD_SETTING_ENABLE (1<<0)
#define MD_SETTING_RELOAD (1<<1)
#define MD_SETTING_FIRST_BOOT (1<<2)	/* this is the first time of boot up */
#define MD_SETTING_DUMMY  (1<<7)

struct ccci_mem_layout {	/* all from AP view, AP has no haredware remap after MT6592 */
	/* MD image */
	void __iomem *md_region_vir;
	phys_addr_t md_region_phy;
	unsigned int md_region_size;
	/* DSP image */
	void __iomem *dsp_region_vir;
	phys_addr_t dsp_region_phy;
	unsigned int dsp_region_size;
	/* Share memory */
	void __iomem *smem_region_vir;
	phys_addr_t smem_region_phy;
	unsigned int smem_region_size;
	unsigned int smem_offset_AP_to_MD; /* offset between AP and MD view of share memory */
	/*MD1 MD3 shared memory*/
	void __iomem *md1_md3_smem_vir;
	phys_addr_t md1_md3_smem_phy;
	unsigned int md1_md3_smem_size;
};

struct ccci_smem_layout {
	/* total exception region */
	void __iomem *ccci_exp_smem_base_vir;
	phys_addr_t ccci_exp_smem_base_phy;
	unsigned int ccci_exp_smem_size;
	unsigned int ccci_exp_dump_size;

	/* runtime data and mpu region */
	void __iomem *ccci_rt_smem_base_vir;
	phys_addr_t ccci_rt_smem_base_phy;
	unsigned int ccci_rt_smem_size;

	/* CCISM region */
	void __iomem *ccci_ccism_smem_base_vir;
	phys_addr_t ccci_ccism_smem_base_phy;
	unsigned int ccci_ccism_smem_size;
	unsigned int ccci_ccism_dump_size;

	/* DHL share memory region */
	void __iomem *ccci_ccb_dhl_base_vir;
	phys_addr_t ccci_ccb_dhl_base_phy;
	unsigned int ccci_ccb_dhl_size;
	void __iomem *ccci_raw_dhl_base_vir;
	phys_addr_t ccci_raw_dhl_base_phy;
	unsigned int ccci_raw_dhl_size;

	/* direct tethering region */
	void __iomem *ccci_dt_netd_smem_base_vir;
	phys_addr_t ccci_dt_netd_smem_base_phy;
	unsigned int ccci_dt_netd_smem_size;
	void __iomem *ccci_dt_usb_smem_base_vir;
	phys_addr_t ccci_dt_usb_smem_base_phy;
	unsigned int ccci_dt_usb_smem_size;

	/* smart logging region */
	void __iomem *ccci_smart_logging_base_vir;
	phys_addr_t ccci_smart_logging_base_phy;
	unsigned int ccci_smart_logging_size;

	/* CCIF share memory region */
	void __iomem *ccci_ccif_smem_base_vir;
	phys_addr_t ccci_ccif_smem_base_phy;
	unsigned int ccci_ccif_smem_size;

	/* sub regions in exception region */
	void __iomem *ccci_exp_smem_ccci_debug_vir;
	unsigned int ccci_exp_smem_ccci_debug_size;
	void __iomem *ccci_exp_smem_mdss_debug_vir;
	unsigned int ccci_exp_smem_mdss_debug_size;
	void __iomem *ccci_exp_smem_sleep_debug_vir;
	unsigned int ccci_exp_smem_sleep_debug_size;
	void __iomem *ccci_exp_smem_dbm_debug_vir;
	void __iomem *ccci_exp_smem_force_assert_vir;
	unsigned int ccci_exp_smem_force_assert_size;
};

typedef enum {
	DUMP_FLAG_CCIF = (1 << 0),
	DUMP_FLAG_CLDMA = (1 << 1),	/* tricky part, use argument length as queue index */
	DUMP_FLAG_REG = (1 << 2),
	DUMP_FLAG_SMEM_EXP = (1 << 3),
	DUMP_FLAG_IMAGE = (1 << 4),
	DUMP_FLAG_LAYOUT = (1 << 5),
	DUMP_FLAG_QUEUE_0 = (1 << 6),
	DUMP_FLAG_QUEUE_0_1 = (1 << 7),
	DUMP_FLAG_CCIF_REG = (1 << 8),
	DUMP_FLAG_SMEM_MDSLP = (1 << 9),
	DUMP_FLAG_MD_WDT = (1 << 10),
	DUMP_FLAG_SMEM_CCISM = (1<<11),
	DUMP_MD_BOOTUP_STATUS = (1<<12),
	DUMP_FLAG_IRQ_STATUS = (1<<13),
} MODEM_DUMP_FLAG;

typedef enum {
	EE_FLAG_ENABLE_WDT = (1 << 0),
	EE_FLAG_DISABLE_WDT = (1 << 1),
} MODEM_EE_FLAG;

#define MD_IMG_DUMP_SIZE (1<<8)
#define DSP_IMG_DUMP_SIZE (1<<9)

typedef enum {
	LOW_BATTERY,
	BATTERY_PERCENT,
} LOW_POEWR_NOTIFY_TYPE;

typedef enum {
	MD_FORCE_ASSERT_RESERVE = 0x000,
	MD_FORCE_ASSERT_BY_MD_NO_RESPONSE	= 0x100,
	MD_FORCE_ASSERT_BY_MD_SEQ_ERROR		= 0x200,
	MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED	= 0x300,
	MD_FORCE_ASSERT_BY_USER_TRIGGER		= 0x400,
	MD_FORCE_ASSERT_BY_MD_WDT			= 0x500,
	MD_FORCE_ASSERT_BY_AP_MPU			= 0x600,
} MD_FORCE_ASSERT_TYPE;

typedef enum {
	CCCI_MESSAGE,
	CCIF_INTERRUPT,
	CCIF_MPU_INTR,
} MD_COMM_TYPE;

typedef enum {
	MD_STATUS_POLL_BUSY = (1 << 0),
	MD_STATUS_ASSERTED = (1 << 1),
} MD_STATUS_POLL_FLAG;

typedef enum {
	OTHER_MD_NONE,
	OTHER_MD_STOP,
	OTHER_MD_RESET,
} OTHER_MD_OPS;

struct ccci_force_assert_shm_fmt {
	unsigned int  error_code;
	unsigned int  param[3];
	unsigned char reserved[0];
};


typedef void __iomem *(*smem_sub_region_cb_t)(void *md_blk, int *size_o);

/****************handshake v2*************/
typedef enum{
	BOOT_INFO = 0,
	EXCEPTION_SHARE_MEMORY,
	CCIF_SHARE_MEMORY,
	SMART_LOGGING_SHARE_MEMORY,
	MD1MD3_SHARE_MEMORY,
	/*ccci misc info*/
	MISC_INFO_HIF_DMA_REMAP,
	MISC_INFO_RTC_32K_LESS,
	MISC_INFO_RANDOM_SEED_NUM,
	MISC_INFO_GPS_COCLOCK,
	MISC_INFO_SBP_ID,
	MISC_INFO_CCCI,	/*=10*/
	MISC_INFO_CLIB_TIME,
	MISC_INFO_C2K,
	MD_IMAGE_START_MEMORY,
	CCISM_SHARE_MEMORY,
	CCB_SHARE_MEMORY, /* total size of all CCB regions */
	DHL_RAW_SHARE_MEMORY,
	DT_NETD_SHARE_MEMORY,
	DT_USB_SHARE_MEMORY,
	EE_AFTER_EPOF,
	CCMNI_MTU, /* max Rx packet buffer size on AP side */
	MISC_INFO_CUSTOMER_VAL = 21,
	CCCI_FAST_HEADER = 22,
	MISC_INFO_C2K_MEID = 24,
	MD_RUNTIME_FEATURE_ID_MAX,
} MD_CCCI_RUNTIME_FEATURE_ID;

typedef enum{
	AT_CHANNEL_NUM = 0,
	AP_RUNTIME_FEATURE_ID_MAX,
} AP_CCCI_RUNTIME_FEATURE_ID;

typedef enum{
	CCCI_FEATURE_NOT_EXIST = 0,
	CCCI_FEATURE_NOT_SUPPORT = 1,
	CCCI_FEATURE_MUST_SUPPORT = 2,
	CCCI_FEATURE_OPTIONAL_SUPPORT = 3,
	CCCI_FEATURE_SUPPORT_BACKWARD_COMPAT = 4,
} CCCI_RUNTIME_FEATURE_SUPPORT_TYPE;

struct ccci_feature_support {
	u8 support_mask:4;
	u8 version:4;
};

struct ccci_runtime_feature {
	u8 feature_id;	/*for debug only*/
	struct ccci_feature_support support_info;
	u8 reserved[2];
	u32 data_len;
	u8 data[0];
};

struct ccci_runtime_boot_info {
	u32 boot_channel;
	u32 booting_start_id;
	u32 boot_attributes;
	u32 boot_ready_id;
};

struct ccci_runtime_share_memory {
	u32 addr;
	u32 size;
};

struct ccci_misc_info_element {
	u32 feature[4];
};

#define FEATURE_COUNT 64
#define MD_FEATURE_QUERY_PATTERN 0x49434343
#define AP_FEATURE_QUERY_PATTERN 0x43434349

struct md_query_ap_feature {
	u32 head_pattern;
	struct ccci_feature_support feature_set[FEATURE_COUNT];
	u32 tail_pattern;
};

struct ap_query_md_feature {
	u32 head_pattern;
	struct ccci_feature_support feature_set[FEATURE_COUNT];
	u32 share_memory_support;
	u32 ap_runtime_data_addr;
	u32 ap_runtime_data_size;
	u32 md_runtime_data_addr;
	u32 md_runtime_data_size;
	u32 set_md_mpu_start_addr;
	u32 set_md_mpu_total_size;
	u32 tail_pattern;
};
/*********************************************/
typedef enum {
	MD_BOOT_MODE_INVALID = 0,
	MD_BOOT_MODE_NORMAL,
	MD_BOOT_MODE_META,
	MD_BOOT_MODE_MAX,
} MD_BOOT_MODE;

enum {
	CRIT_USR_FS,
	CRIT_USR_MUXD,
	CRIT_USR_MDLOG,
	CRIT_USR_META,
	CRIT_USR_MDLOG_CTRL = CRIT_USR_META,
	CRIT_USR_MAX,
};

enum {
	MD_DBG_DUMP_INVALID = -1,
	MD_DBG_DUMP_TOPSM = 0,
	MD_DBG_DUMP_PCMON,
	MD_DBG_DUMP_BUSREC,
	MD_DBG_DUMP_MDRGU,
	MD_DBG_DUMP_OST,
	MD_DBG_DUMP_BUS,
	MD_DBG_DUMP_PLL,
	MD_DBG_DUMP_ECT,

	MD_DBG_DUMP_SMEM,
	MD_DBG_DUMP_PORT = 0x60000000,
	MD_DBG_DUMP_ALL = 0x7FFFFFFF,
};

enum {
	MD_CFG_MDLOG_MODE,
	MD_CFG_SBP_CODE,
	MD_CFG_DUMP_FLAG,
};

enum {
	ALL_CCIF = 0,
	AP_MD1_CCIF,
	AP_MD3_CCIF,
	MD1_MD3_CCIF,
};

#ifdef FEATURE_MD_GET_CLIB_TIME
extern volatile int current_time_zone;
#endif

int ccci_register_dev_node(const char *name, int major_id, int minor);
int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf, unsigned int len);
int ccci_scp_ipi_send(int md_id, int op_id, void *data);
void ccci_sysfs_add_md(int md_id, void *kobj);
void scp_md_state_sync_work(struct work_struct *work);

/* common sub-system */
extern int ccci_subsys_bm_init(void);
extern int ccci_subsys_sysfs_init(void);
extern int ccci_subsys_dfo_init(void);
/* per-modem sub-system */
extern int switch_MD1_Tx_Power(unsigned int mode);
extern int switch_MD2_Tx_Power(unsigned int mode);

#ifdef FEATURE_MTK_SWITCH_TX_POWER
int swtp_init(int md_id);
#endif

/* ================================================================================= */
/* IOCTL defination */
/* ================================================================================= */
/* CCCI == EEMCS */
#define CCCI_IOC_MAGIC 'C'
#define CCCI_IOC_MD_RESET			_IO(CCCI_IOC_MAGIC, 0) /* mdlogger // META // muxreport */
#define CCCI_IOC_GET_MD_STATE			_IOR(CCCI_IOC_MAGIC, 1, unsigned int) /* audio */
#define CCCI_IOC_PCM_BASE_ADDR			_IOR(CCCI_IOC_MAGIC, 2, unsigned int) /* audio */
#define CCCI_IOC_PCM_LEN			_IOR(CCCI_IOC_MAGIC, 3, unsigned int) /* audio */
#define CCCI_IOC_FORCE_MD_ASSERT		_IO(CCCI_IOC_MAGIC, 4) /* muxreport // mdlogger */
#define CCCI_IOC_ALLOC_MD_LOG_MEM		_IO(CCCI_IOC_MAGIC, 5) /* mdlogger */
#define CCCI_IOC_DO_MD_RST			_IO(CCCI_IOC_MAGIC, 6) /* md_init */
#define CCCI_IOC_SEND_RUN_TIME_DATA		_IO(CCCI_IOC_MAGIC, 7) /* md_init */
#define CCCI_IOC_GET_MD_INFO			_IOR(CCCI_IOC_MAGIC, 8, unsigned int) /* md_init */
#define CCCI_IOC_GET_MD_EX_TYPE			_IOR(CCCI_IOC_MAGIC, 9, unsigned int) /* mdlogger */
#define CCCI_IOC_SEND_STOP_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 10) /* muxreport */
#define CCCI_IOC_SEND_START_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 11) /* muxreport */
#define CCCI_IOC_DO_STOP_MD			_IO(CCCI_IOC_MAGIC, 12) /* md_init */
#define CCCI_IOC_DO_START_MD			_IO(CCCI_IOC_MAGIC, 13) /* md_init */
#define CCCI_IOC_ENTER_DEEP_FLIGHT		_IO(CCCI_IOC_MAGIC, 14) /* RILD // factory */
#define CCCI_IOC_LEAVE_DEEP_FLIGHT		_IO(CCCI_IOC_MAGIC, 15) /* RILD // factory */
#define CCCI_IOC_POWER_ON_MD			_IO(CCCI_IOC_MAGIC, 16) /* md_init, abandoned */
#define CCCI_IOC_POWER_OFF_MD			_IO(CCCI_IOC_MAGIC, 17) /* md_init, abandoned */
#define CCCI_IOC_POWER_ON_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 18) /* md_init, abandoned */
#define CCCI_IOC_POWER_OFF_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 19) /* md_init, abandoned */
#define CCCI_IOC_SIM_SWITCH			_IOW(CCCI_IOC_MAGIC, 20, unsigned int) /* RILD // factory */
#define CCCI_IOC_SEND_BATTERY_INFO		_IO(CCCI_IOC_MAGIC, 21) /* md_init */
#define CCCI_IOC_SIM_SWITCH_TYPE		_IOR(CCCI_IOC_MAGIC, 22, unsigned int) /* RILD */
#define CCCI_IOC_STORE_SIM_MODE			_IOW(CCCI_IOC_MAGIC, 23, unsigned int) /* RILD */
#define CCCI_IOC_GET_SIM_MODE			_IOR(CCCI_IOC_MAGIC, 24, unsigned int) /* RILD */
#define CCCI_IOC_RELOAD_MD_TYPE			_IO(CCCI_IOC_MAGIC, 25) /* META // md_init // muxreport */
#define CCCI_IOC_GET_SIM_TYPE			_IOR(CCCI_IOC_MAGIC, 26, unsigned int) /* terservice */
#define CCCI_IOC_ENABLE_GET_SIM_TYPE		_IOW(CCCI_IOC_MAGIC, 27, unsigned int) /* terservice */
#define CCCI_IOC_SEND_ICUSB_NOTIFY		_IOW(CCCI_IOC_MAGIC, 28, unsigned int) /* icusbd */
#define CCCI_IOC_SET_MD_IMG_EXIST		_IOW(CCCI_IOC_MAGIC, 29, unsigned int) /* md_init */
#define CCCI_IOC_GET_MD_IMG_EXIST		_IOR(CCCI_IOC_MAGIC, 30, unsigned int) /* META */
#define CCCI_IOC_GET_MD_TYPE			_IOR(CCCI_IOC_MAGIC, 31, unsigned int) /* RILD */
#define CCCI_IOC_STORE_MD_TYPE			_IOW(CCCI_IOC_MAGIC, 32, unsigned int) /* RILD */
#define CCCI_IOC_GET_MD_TYPE_SAVING		_IOR(CCCI_IOC_MAGIC, 33, unsigned int) /* META */
#define CCCI_IOC_GET_EXT_MD_POST_FIX		_IOR(CCCI_IOC_MAGIC, 34, unsigned int) /* mdlogger */
#define CCCI_IOC_FORCE_FD			_IOW(CCCI_IOC_MAGIC, 35, unsigned int) /* RILD(6577) */
#define CCCI_IOC_AP_ENG_BUILD			_IOW(CCCI_IOC_MAGIC, 36, unsigned int) /* md_init(6577) */
#define CCCI_IOC_GET_MD_MEM_SIZE		_IOR(CCCI_IOC_MAGIC, 37, unsigned int) /* md_init(6577) */
#define CCCI_IOC_UPDATE_SIM_SLOT_CFG		_IOW(CCCI_IOC_MAGIC, 38, unsigned int) /* RILD */
#define CCCI_IOC_GET_CFG_SETTING		_IOW(CCCI_IOC_MAGIC, 39, unsigned int) /* md_init */

#define CCCI_IOC_SET_MD_SBP_CFG			_IOW(CCCI_IOC_MAGIC, 40, unsigned int) /* md_init */
#define CCCI_IOC_GET_MD_SBP_CFG			_IOW(CCCI_IOC_MAGIC, 41, unsigned int) /* md_init */
/* modem protocol type for meta: AP_TST or DHL */
#define CCCI_IOC_GET_MD_PROTOCOL_TYPE	_IOR(CCCI_IOC_MAGIC, 42, char[16])
#define CCCI_IOC_SEND_SIGNAL_TO_USER	_IOW(CCCI_IOC_MAGIC, 43, unsigned int) /* md_init */
#define CCCI_IOC_RESET_MD1_MD3_PCCIF	_IO(CCCI_IOC_MAGIC, 45) /* md_init */
#define CCCI_IOC_RESET_AP               _IOW(CCCI_IOC_MAGIC, 46, unsigned int)

/*md_init set MD boot env data before power on MD */
#define CCCI_IOC_SET_BOOT_DATA			_IOW(CCCI_IOC_MAGIC, 47, unsigned int[16])

/* for user space share memory user */
#define CCCI_IOC_SMEM_BASE			_IOR(CCCI_IOC_MAGIC, 48, unsigned int)
#define CCCI_IOC_SMEM_LEN			_IOR(CCCI_IOC_MAGIC, 49, unsigned int)
#define CCCI_IOC_SMEM_TX_NOTIFY			_IOW(CCCI_IOC_MAGIC, 50, unsigned int)
#define CCCI_IOC_SMEM_RX_POLL			_IOR(CCCI_IOC_MAGIC, 51, unsigned int)
#define CCCI_IOC_SMEM_SET_STATE			_IOW(CCCI_IOC_MAGIC, 52, unsigned int)
#define CCCI_IOC_SMEM_GET_STATE			_IOR(CCCI_IOC_MAGIC, 53, unsigned int)

#define CCCI_IOC_SET_CCIF_CG			_IOW(CCCI_IOC_MAGIC, 54, unsigned int) /*md_init*/
#define CCCI_IOC_SET_EFUN               _IOW(CCCI_IOC_MAGIC, 55, unsigned int) /* RILD */
#define CCCI_IOC_MDLOG_DUMP_DONE		_IO(CCCI_IOC_MAGIC, 56) /*mdlogger*/
#define CCCI_IOC_GET_OTHER_MD_STATE		_IOR(CCCI_IOC_MAGIC, 57, unsigned int) /* mdlogger */
#define CCCI_IOC_SET_MD_BOOT_MODE       _IOW(CCCI_IOC_MAGIC, 58, unsigned int) /* META */
#define CCCI_IOC_GET_MD_BOOT_MODE       _IOR(CCCI_IOC_MAGIC, 59, unsigned int) /* md_init */

#define CCCI_IOC_GET_AT_CH_NUM			_IOR(CCCI_IOC_MAGIC, 60, unsigned int) /* RILD */
#define CCCI_IOC_ENTER_UPLOAD		_IO(CCCI_IOC_MAGIC, 61) /* modem log for S */
#define CCCI_IOC_GET_RAT_STR			_IOR(CCCI_IOC_MAGIC, 62, unsigned int[16])
#define CCCI_IOC_SET_RAT_STR			_IOW(CCCI_IOC_MAGIC, 63, unsigned int[16])

#define CCCI_IOC_SET_HEADER				_IO(CCCI_IOC_MAGIC,  112) /* emcs_va */
#define CCCI_IOC_CLR_HEADER				_IO(CCCI_IOC_MAGIC,  113) /* emcs_va */
#define CCCI_IOC_DL_TRAFFIC_CONTROL		_IOW(CCCI_IOC_MAGIC, 119, unsigned int) /* mdlogger */

#define CCCI_IPC_MAGIC 'P' /* only for IPC user */
/* CCCI == EEMCS */
#define CCCI_IPC_RESET_RECV			_IO(CCCI_IPC_MAGIC, 0)
#define CCCI_IPC_RESET_SEND			_IO(CCCI_IPC_MAGIC, 1)
#define CCCI_IPC_WAIT_MD_READY			_IO(CCCI_IPC_MAGIC, 2)
#define CCCI_IPC_KERN_WRITE_TEST		_IO(CCCI_IPC_MAGIC, 3)
#define CCCI_IPC_UPDATE_TIME			_IO(CCCI_IPC_MAGIC, 4)
#define CCCI_IPC_WAIT_TIME_UPDATE		_IO(CCCI_IPC_MAGIC, 5)
#define CCCI_IPC_UPDATE_TIMEZONE		_IO(CCCI_IPC_MAGIC, 6)

/* ================================================================================= */
/* CCCI Channel ID and Message defination */
/* ================================================================================= */
typedef enum {
	CCCI_CONTROL_RX = 0,
	CCCI_CONTROL_TX = 1,
	CCCI_SYSTEM_RX = 2,
	CCCI_SYSTEM_TX = 3,
	CCCI_PCM_RX = 4,
	CCCI_PCM_TX = 5,
	CCCI_UART1_RX = 6, /* META */
	CCCI_UART1_RX_ACK = 7,
	CCCI_UART1_TX = 8,
	CCCI_UART1_TX_ACK = 9,
	CCCI_UART2_RX = 10, /* MUX */
	CCCI_UART2_RX_ACK = 11,
	CCCI_UART2_TX = 12,
	CCCI_UART2_TX_ACK = 13,
	CCCI_FS_RX = 14,
	CCCI_FS_TX = 15,
	CCCI_PMIC_RX = 16,
	CCCI_PMIC_TX = 17,
	CCCI_UEM_RX = 18,
	CCCI_UEM_TX = 19,
	CCCI_CCMNI1_RX = 20,
	CCCI_CCMNI1_RX_ACK = 21,
	CCCI_CCMNI1_TX = 22,
	CCCI_CCMNI1_TX_ACK = 23,
	CCCI_CCMNI2_RX = 24,
	CCCI_CCMNI2_RX_ACK = 25,
	CCCI_CCMNI2_TX = 26,
	CCCI_CCMNI2_TX_ACK = 27,
	CCCI_CCMNI3_RX = 28,
	CCCI_CCMNI3_RX_ACK = 29,
	CCCI_CCMNI3_TX = 30,
	CCCI_CCMNI3_TX_ACK = 31,
	CCCI_RPC_RX = 32,
	CCCI_RPC_TX = 33,
	CCCI_IPC_RX = 34,
	CCCI_IPC_RX_ACK = 35,
	CCCI_IPC_TX = 36,
	CCCI_IPC_TX_ACK = 37,
	CCCI_IPC_UART_RX = 38,
	CCCI_IPC_UART_RX_ACK = 39,
	CCCI_IPC_UART_TX = 40,
	CCCI_IPC_UART_TX_ACK = 41,
	CCCI_MD_LOG_RX = 42,
	CCCI_MD_LOG_TX = 43,
	/* ch44~49 reserved for ARM7 */
	CCCI_IT_RX = 50,
	CCCI_IT_TX = 51,
	CCCI_IMSV_UL = 52,
	CCCI_IMSV_DL = 53,
	CCCI_IMSC_UL = 54,
	CCCI_IMSC_DL = 55,
	CCCI_IMSA_UL = 56,
	CCCI_IMSA_DL = 57,
	CCCI_IMSDC_UL = 58,
	CCCI_IMSDC_DL = 59,
	CCCI_ICUSB_RX = 60,
	CCCI_ICUSB_TX = 61,
	CCCI_LB_IT_RX = 62,
	CCCI_LB_IT_TX = 63,
	CCCI_CCMNI1_DL_ACK = 64,
	CCCI_CCMNI2_DL_ACK = 65,
	CCCI_CCMNI3_DL_ACK = 66,
	CCCI_STATUS_RX = 67,
	CCCI_STATUS_TX = 68,
	CCCI_CCMNI4_RX                  = 69,
	CCCI_CCMNI4_RX_ACK              = 70,
	CCCI_CCMNI4_TX                  = 71,
	CCCI_CCMNI4_TX_ACK              = 72,
	CCCI_CCMNI4_DLACK_RX            = 73,
	CCCI_CCMNI5_RX                  = 74,
	CCCI_CCMNI5_RX_ACK              = 75,
	CCCI_CCMNI5_TX                  = 76,
	CCCI_CCMNI5_TX_ACK              = 77,
	CCCI_CCMNI5_DLACK_RX            = 78,
	CCCI_CCMNI6_RX                  = 79,
	CCCI_CCMNI6_RX_ACK              = 80,
	CCCI_CCMNI6_TX                  = 81,
	CCCI_CCMNI6_TX_ACK              = 82,
	CCCI_CCMNI6_DLACK_RX            = 83,
	CCCI_CCMNI7_RX                  = 84,
	CCCI_CCMNI7_RX_ACK              = 85,
	CCCI_CCMNI7_TX                  = 86,
	CCCI_CCMNI7_TX_ACK              = 87,
	CCCI_CCMNI7_DLACK_RX            = 88,
	CCCI_CCMNI8_RX                  = 89,
	CCCI_CCMNI8_RX_ACK              = 90,
	CCCI_CCMNI8_TX                  = 91,
	CCCI_CCMNI8_TX_ACK              = 92,
	CCCI_CCMNI8_DLACK_RX            = 93,
	CCCI_MDL_MONITOR_DL             = 94,
	CCCI_MDL_MONITOR_UL             = 95,
	CCCI_CCMNILAN_RX                = 96,
	CCCI_CCMNILAN_RX_ACK            = 97,
	CCCI_CCMNILAN_TX                = 98,
	CCCI_CCMNILAN_TX_ACK            = 99,
	CCCI_CCMNILAN_DLACK_RX          = 100,
	CCCI_IMSEM_UL                   = 101,
	CCCI_IMSEM_DL                   = 102,
	CCCI_CCMNI10_RX                 = 103,
	CCCI_CCMNI10_RX_ACK             = 104,
	CCCI_CCMNI10_TX                 = 105,
	CCCI_CCMNI10_TX_ACK             = 106,
	CCCI_CCMNI10_DLACK_RX           = 107,
	CCCI_CCMNI11_RX                 = 108,
	CCCI_CCMNI11_RX_ACK             = 109,
	CCCI_CCMNI11_TX                 = 110,
	CCCI_CCMNI11_TX_ACK             = 111,
	CCCI_CCMNI11_DLACK_RX           = 112,
	CCCI_CCMNI12_RX                 = 113,
	CCCI_CCMNI12_RX_ACK             = 114,
	CCCI_CCMNI12_TX                 = 115,
	CCCI_CCMNI12_TX_ACK             = 116,
	CCCI_CCMNI12_DLACK_RX           = 117,
	CCCI_CCMNI13_RX                 = 118,
	CCCI_CCMNI13_RX_ACK             = 119,
	CCCI_CCMNI13_TX                 = 120,
	CCCI_CCMNI13_TX_ACK             = 121,
	CCCI_CCMNI13_DLACK_RX           = 122,
	CCCI_CCMNI14_RX                 = 123,
	CCCI_CCMNI14_RX_ACK             = 124,
	CCCI_CCMNI14_TX                 = 125,
	CCCI_CCMNI14_TX_ACK             = 126,
	CCCI_CCMNI14_DLACK_RX           = 127,
	CCCI_CCMNI15_RX                 = 128,
	CCCI_CCMNI15_RX_ACK             = 129,
	CCCI_CCMNI15_TX                 = 130,
	CCCI_CCMNI15_TX_ACK             = 131,
	CCCI_CCMNI15_DLACK_RX           = 132,
	CCCI_CCMNI16_RX                 = 133,
	CCCI_CCMNI16_RX_ACK             = 134,
	CCCI_CCMNI16_TX                 = 135,
	CCCI_CCMNI16_TX_ACK             = 136,
	CCCI_CCMNI16_DLACK_RX           = 137,
	CCCI_CCMNI17_RX                 = 138,
	CCCI_CCMNI17_RX_ACK             = 139,
	CCCI_CCMNI17_TX                 = 140,
	CCCI_CCMNI17_TX_ACK             = 141,
	CCCI_CCMNI17_DLACK_RX           = 142,
	CCCI_CCMNI18_RX                 = 143,
	CCCI_CCMNI18_RX_ACK             = 144,
	CCCI_CCMNI18_TX                 = 145,
	CCCI_CCMNI18_TX_ACK             = 146,
	CCCI_CCMNI18_DLACK_RX           = 147,

	/*5 chs for C2K only*/
	CCCI_C2K_PPP_DATA,		/* data ch for c2k */
	CCCI_C2K_AT,	/*rild AT ch for c2k*/
	CCCI_C2K_AT2,	/*rild AT2 ch for c2k*/
	CCCI_C2K_AT3,	/*rild AT3 ch for c2k*/
	CCCI_C2K_AT4,	/*rild AT4 ch for c2k*/
	CCCI_C2K_AT5,	/*rild AT5 ch for c2k*/
	CCCI_C2K_AT6,	/*rild AT6 ch for c2k*/
	CCCI_C2K_AT7,	/*rild AT7 ch for c2k*/
	CCCI_C2K_AT8,	/*rild AT8 ch for c2k*/
	CCCI_C2K_LB_DL,	/*downlink loopback*/

	/* virtual channels */
	CCCI_MONITOR_CH,
	CCCI_DUMMY_CH,
	CCCI_SMEM_CH,
	CCCI_MAX_CH_NUM, /* RX channel ID should NOT be >= this!! */
	CCCI_OVER_MAX_CH,

	CCCI_MONITOR_CH_ID = 0xf0000000, /* for backward compatible */
	CCCI_FORCE_ASSERT_CH = 20090215,
	CCCI_INVALID_CH_ID = 0xffffffff,
} CCCI_CH;

enum c2k_channel {
	CTRL_CH_C2K = 0,
	AUDIO_CH_C2K = 1,
	DATA_PPP_CH_C2K = 2,
	MDLOG_CTRL_CH_C2K = 3,
	FS_CH_C2K = 4,
	AT_CH_C2K = 5,
	AGPS_CH_C2K = 6,
	AT2_CH_C2K = 7,
	AT3_CH_C2K = 8,
	MDLOG_CH_C2K = 9,
	AT4_CH_C2K = 10,
	STATUS_CH_C2K = 11,
	NET1_CH_C2K = 12,
	NET2_CH_C2K = 13,	/*need sync with c2k */
	NET3_CH_C2K = 14,	/*need sync with c2k */
	NET4_CH_C2K = 15,
	NET5_CH_C2K = 16,
	NET6_CH_C2K = 17,	/*need sync with c2k */
	NET7_CH_C2K = 18,
	NET8_CH_C2K = 19,
	AT5_CH_C2K = 20,
	AT6_CH_C2K = 21,
	AT7_CH_C2K = 22,
	AT8_CH_C2K = 23,

	C2K_MAX_CH_NUM,
	C2K_OVER_MAX_CH,

	LOOPBACK_C2K = 255,
	MD2AP_LOOPBACK_C2K = 256,
};

/* AP->md_init messages on monitor channel */
typedef enum {
	CCCI_MD_MSG_FORCE_STOP_REQUEST = 0xFAF50001,
	CCCI_MD_MSG_FLIGHT_STOP_REQUEST,
	CCCI_MD_MSG_FORCE_START_REQUEST,
	CCCI_MD_MSG_FLIGHT_START_REQUEST,
	CCCI_MD_MSG_RESET_REQUEST,

	CCCI_MD_MSG_EXCEPTION,
	CCCI_MD_MSG_SEND_BATTERY_INFO,
	CCCI_MD_MSG_STORE_NVRAM_MD_TYPE,
	CCCI_MD_MSG_CFG_UPDATE,
	CCCI_MD_MSG_RANDOM_PATTERN,
} CCCI_MD_MSG;

/* AP<->MD messages on control or system channel */
enum {
	/* Control channel, MD->AP */
	MD_INIT_START_BOOT = 0x0,
	MD_NORMAL_BOOT = 0x0,
	MD_NORMAL_BOOT_READY = 0x1, /* not using */
	MD_META_BOOT_READY = 0x2, /* not using */
	MD_RESET = 0x3, /* not using */
	MD_EX = 0x4,
	CCCI_DRV_VER_ERROR = 0x5,
	MD_EX_REC_OK = 0x6,
	MD_EX_RESUME = 0x7, /* not using */
	MD_EX_PASS = 0x8,
	MD_INIT_CHK_ID = 0x5555FFFF,
	MD_EX_CHK_ID = 0x45584350,
	MD_EX_REC_OK_CHK_ID = 0x45524543,

	/* System channel, AP->MD || AP<-->MD message start from 0x100 */
	MD_DORMANT_NOTIFY = 0x100,
	MD_SLP_REQUEST = 0x101,
	MD_TX_POWER = 0x102,
	MD_RF_TEMPERATURE = 0x103,
	MD_RF_TEMPERATURE_3G = 0x104,
	MD_GET_BATTERY_INFO = 0x105,

	MD_SIM_TYPE = 0x107,
	MD_ICUSB_NOTIFY = 0x108,
	/* 0x109 for md legacy use to crystal_thermal_change */
	MD_LOW_BATTERY_LEVEL = 0x10A,
	/* 0x10B-0x10C occupied by EEMCS */
	MD_PAUSE_LTE = 0x10D,
	MD_RESET_AP = 0x118,
	/* swtp */
	MD_SW_MD1_TX_POWER = 0x10E,
	MD_SW_MD2_TX_POWER = 0x10F,
	MD_SW_MD1_TX_POWER_REQ = 0x110,
	MD_SW_MD2_TX_POWER_REQ = 0x111,
	MD_THROTTLING = 0x112, /* SW throughput throttling */
	/* TEST_MESSAGE for IT only */
	TEST_MSG_ID_MD2AP = 0x114,
	TEST_MSG_ID_AP2MD = 0x115,
	TEST_MSG_ID_L1CORE_MD2AP = 0x116,
	TEST_MSG_ID_L1CORE_AP2MD = 0x117,

	CCISM_SHM_INIT = 0x119,
	CCISM_SHM_INIT_ACK = 0x11A,
	CCISM_SHM_INIT_DONE = 0x11B,
	PMIC_INTR_MODEM_BUCK_OC = 0x11C,
	MD_AP_MPU_ACK_MD = 0x11D,

	/*c2k ctrl msg start from 0x200*/
	C2K_STATUS_IND_MSG = 0x201, /* for usb bypass */
	C2K_STATUS_QUERY_MSG = 0x202, /* for usb bypass */
	C2K_FLOW_CTRL_MSG = 0x205,
	C2K_HB_MSG = 0x207,
	C2K_CCISM_SHM_INIT = 0x209,
	C2K_CCISM_SHM_INIT_ACK = 0x20A,
	C2K_CCISM_SHM_INIT_DONE = 0x20B,

	/* System channel, MD->AP message start from 0x1000 */
	MD_WDT_MONITOR = 0x1000,
	/* System channel, AP->MD message */
	MD_WAKEN_UP = 0x10000,
};

#define NORMAL_BOOT_ID 0
#define META_BOOT_ID 1
#define FACTORY_BOOT_ID	2

struct ccci_dev_cfg {
	unsigned int index;
	unsigned int major;
	unsigned int minor_base;
	unsigned int capability;
};

/* Rutime data common part */
typedef enum {
	FEATURE_NOT_EXIST = 0,
	FEATURE_NOT_SUPPORT,
	FEATURE_SUPPORT,
	FEATURE_PARTIALLY_SUPPORT,
} MISC_FEATURE_STATE;

typedef enum {
	MISC_DMA_ADDR = 0,
	MISC_32K_LESS,
	MISC_RAND_SEED,
	MISC_MD_COCLK_SETTING,
	MISC_MD_SBP_SETTING,
	MISC_MD_SEQ_CHECK,
	MISC_MD_CLIB_TIME,
	MISC_MD_C2K_ON,
} MISC_FEATURE_ID;

typedef enum {
	MODE_UNKNOWN = -1,	  /* -1 */
	MODE_IDLE,			  /* 0 */
	MODE_USB,			   /* 1 */
	MODE_SD,				/* 2 */
	MODE_POLLING,		   /* 3 */
	MODE_WAITSD,			/* 4 */
} LOGGING_MODE;

typedef enum {
	HIF_EX_INIT = 0, /* interrupt */
	HIF_EX_ACK, /* AP->MD */
	HIF_EX_INIT_DONE, /* polling */
	HIF_EX_CLEARQ_DONE, /* interrupt */
	HIF_EX_CLEARQ_ACK, /* AP->MD */
	HIF_EX_ALLQ_RESET, /* polling */
} HIF_EX_STAGE;

enum {
	P_CORE = 0,
	VOLTE_CORE,
};

/* runtime data format uses EEMCS's version, NOT the same with legacy CCCI */
struct modem_runtime {
	u32 Prefix;			 /* "CCIF" */
	u32 Platform_L;		 /* Hardware Platform String ex: "TK6516E0" */
	u32 Platform_H;
	u32 DriverVersion;	  /* 0x00000923 since W09.23 */
	u32 BootChannel;		/* Channel to ACK AP with boot ready */
	u32 BootingStartID;	 /* MD is booting. NORMAL_BOOT_ID or META_BOOT_ID */
#if 1 /* not using in EEMCS */
	u32 BootAttributes;	 /* Attributes passing from AP to MD Booting */
	u32 BootReadyID;		/* MD response ID if boot successful and ready */
	u32 FileShareMemBase;
	u32 FileShareMemSize;
	u32 ExceShareMemBase;
	u32 ExceShareMemSize;
	u32 CCIFShareMemBase;
	u32 CCIFShareMemSize;
#ifdef FEATURE_SMART_LOGGING
	u32 DHLShareMemBase; /* For DHL */
	u32 DHLShareMemSize;
#endif
#ifdef FEATURE_MD1MD3_SHARE_MEM
	u32 MD1MD3ShareMemBase; /* For MD1 MD3 share memory */
	u32 MD1MD3ShareMemSize;
#endif
	u32 TotalShareMemBase;
	u32 TotalShareMemSize;
	u32 CheckSum;
#endif
	u32 Postfix; /* "CCIF" */
#if 1 /* misc region */
	u32 misc_prefix;	/* "MISC" */
	u32 support_mask;
	u32 index;
	u32 next;
	u32 feature_0_val[4];
	u32 feature_1_val[4];
	u32 feature_2_val[4];
	u32 feature_3_val[4];
	u32 feature_4_val[4];
	u32 feature_5_val[4];
	u32 feature_6_val[4];
	u32 feature_7_val[4];
	u32 feature_8_val[4];
	u32 feature_9_val[4];
	u32 feature_10_val[4];
	u32 feature_11_val[4];
	u32 feature_12_val[4];
	u32 feature_13_val[4];
	u32 feature_14_val[4];
	u32 feature_15_val[4];
	u32 reserved_2[3];
	u32 misc_postfix;	/* "MISC" */
#endif
} __packed;

/* do not modify this c2k structure, because we assume its total size is 32bit,
 * and used as ccci_header's 'reserved' member
 */
struct c2k_ctrl_port_msg {
	unsigned char id_hi;
	unsigned char id_low;
	unsigned char chan_num;
	unsigned char option;
} __packed; /* not necessary, but it's a good gesture, :) */

#endif	/* __CCCI_CORE_H__ */
