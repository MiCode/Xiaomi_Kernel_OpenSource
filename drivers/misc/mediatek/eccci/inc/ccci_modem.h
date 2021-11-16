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

#ifndef __CCCI_MODEM_H__
#define __CCCI_MODEM_H__

#include <mt-plat/mtk_ccci_common.h>

enum MD_FORCE_ASSERT_TYPE {
	MD_FORCE_ASSERT_RESERVE = 0x000,
	MD_FORCE_ASSERT_BY_MD_NO_RESPONSE	= 0x100,
	MD_FORCE_ASSERT_BY_MD_SEQ_ERROR		= 0x200,
	MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED	= 0x300,
	MD_FORCE_ASSERT_BY_USER_TRIGGER		= 0x400,
	MD_FORCE_ASSERT_BY_MD_WDT			= 0x500,
	MD_FORCE_ASSERT_BY_AP_MPU			= 0x600,
};

enum MODEM_DUMP_FLAG {
	DUMP_FLAG_CCIF = (1 << 0),
	/* tricky part, use argument length as queue index */
	DUMP_FLAG_CLDMA = (1 << 1),
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
	DUMP_FLAG_SMEM_CCB_CTRL = (1<<14),
	DUMP_FLAG_SMEM_CCB_DATA = (1<<15),
	DUMP_FLAG_PCCIF_REG = (1 << 16),
};

enum {
	MD_DBG_DUMP_INVALID = -1,
	/*AP side reg 0~15*/
	MD_DBG_DUMP_PCMON = 0,
	MD_DBG_DUMP_BUSREC,
	MD_DBG_DUMP_BUS,
	MD_DBG_DUMP_PLL,
	MD_DBG_DUMP_ECT,

	MD_DBG_DUMP_SMEM,
	/*md side reg 16~23*/
	MD_DBG_DUMP_TOPSM = 16,
	MD_DBG_DUMP_MDRGU,
	MD_DBG_DUMP_OST,
	/*misc*/
	MD_DBG_DUMP_PORT = 24,
	/*bit map*/
	MD_DBG_DUMP_AP_REG = 0xFFFF,
	MD_DBG_DUMP_ALL = 0x7FFFFFFF,
};

enum MD_BOOT_MODE {
	MD_BOOT_MODE_INVALID = 0,
	MD_BOOT_MODE_NORMAL,
	MD_BOOT_MODE_META,
	MD_BOOT_MODE_MAX,
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

enum {
	AP_MD_HS_V2 = 2,
};

enum {
	SMF_CLR_RESET = (1 << 0), /* clear when reset modem */
	SMF_NCLR_FIRST = (1 << 1), /* do not clear even in MD first boot up */
	SMF_MD3_RELATED = (1 << 2), /* MD3 related share memory */
};

struct ccci_mem_region {
	phys_addr_t base_md_view_phy;
	phys_addr_t base_ap_view_phy;
	void __iomem *base_ap_view_vir;
	unsigned int size;
};

struct ccci_smem_region {
	/* pre-defined */
	unsigned int id;
	unsigned int offset; /* in bank4 */
	unsigned int size;
	unsigned int flag;
	/* runtime calculated */
	phys_addr_t base_md_view_phy;
	phys_addr_t base_ap_view_phy;
	void __iomem *base_ap_view_vir;
};

struct ccci_mem_layout {
	/* MD RO and RW (bank0) */
	struct ccci_mem_region md_bank0;

	/* share memory (bank4) */
	struct ccci_mem_region md_bank4_noncacheable_total;
	struct ccci_mem_region md_bank4_cacheable_total;

	/* share memory detail */
	struct ccci_smem_region *md_bank4_noncacheable;
	struct ccci_smem_region *md_bank4_cacheable;
};

enum{
	CCCI_FEATURE_NOT_EXIST = 0,
	CCCI_FEATURE_NOT_SUPPORT = 1,
	CCCI_FEATURE_MUST_SUPPORT = 2,
	CCCI_FEATURE_OPTIONAL_SUPPORT = 3,
	CCCI_FEATURE_SUPPORT_BACKWARD_COMPAT = 4,
}; /* CCCI_RUNTIME_FEATURE_SUPPORT_TYPE */

enum{
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
	AP_CCMNI_MTU, /* max Rx packet buffer size on AP side */
	MISC_INFO_CUSTOMER_VAL = 21,
	CCCI_FAST_HEADER = 22,
	MISC_INFO_C2K_MEID = 24,
	LWA_SHARE_MEMORY = 25,
	AUDIO_RAW_SHARE_MEMORY = 26,
	MULTI_MD_MPU = 27,
	CCISM_SHARE_MEMORY_EXP = 28,
	MD_PHY_CAPTURE = 29,
	MD_CONSYS_SHARE_MEMORY = 30,
	MD_USIP_SHARE_MEMORY = 31,
	MD_MTEE_SHARE_MEMORY_ENABLE = 32,
	MD_POS_SHARE_MEMORY = 33,
	UDC_RAW_SHARE_MEMORY = 34,
	MD_WIFI_PROXY_SHARE_MEMORY = 35,
	NVRAM_CACHE_SHARE_MEMORY = 36,
	SECURITY_SHARE_MEMORY = 37,
	MD_MEM_AP_VIEW_INF = 38,
	MD_RUNTIME_FEATURE_ID_MAX,
}; /* MD_CCCI_RUNTIME_FEATURE_ID; */

enum AP_CCCI_RUNTIME_FEATURE_ID {
	AT_CHANNEL_NUM = 0,
	AP_RUNTIME_FEATURE_ID_MAX,
};

/* Rutime data common part */
enum MISC_FEATURE_STATE {
	FEATURE_NOT_EXIST = 0,
	FEATURE_NOT_SUPPORT,
	FEATURE_SUPPORT,
	FEATURE_PARTIALLY_SUPPORT,
};

enum MISC_FEATURE_ID {
	MISC_DMA_ADDR = 0,
	MISC_32K_LESS,
	MISC_RAND_SEED,
	MISC_MD_COCLK_SETTING,
	MISC_MD_SBP_SETTING,
	MISC_MD_SEQ_CHECK,
	MISC_MD_CLIB_TIME,
	MISC_MD_C2K_ON,
};

struct ccci_feature_support {
	u8 support_mask:4;
	u8 version:4;
};

#define FEATURE_COUNT 64
#define MD_FEATURE_QUERY_PATTERN 0x49434343
#define AP_FEATURE_QUERY_PATTERN 0x43434349
#define CCCI_AP_RUNTIME_RESERVED_SIZE 120
#define CCCI_MD_RUNTIME_RESERVED_SIZE 152

struct md_query_ap_feature {
	u32 head_pattern;
	struct ccci_feature_support feature_set[FEATURE_COUNT];
	u32 tail_pattern;
#if (MD_GENERATION >= 6293)
	u8  reserved[CCCI_MD_RUNTIME_RESERVED_SIZE];
#endif
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
#if (MD_GENERATION >= 6293)
	u8  reserved[CCCI_AP_RUNTIME_RESERVED_SIZE];
#endif
};

struct ap_query_md_feature_v2_1 {
	u32 head_pattern;
	struct ccci_feature_support feature_set[FEATURE_COUNT];
	u32 share_memory_support;
	u32 ap_runtime_data_addr;
	u32 ap_runtime_data_size;
	u32 md_runtime_data_addr;
	u32 md_runtime_data_size;
	u32 noncached_mpu_start_addr;
	u32 noncached_mpu_total_size;
	u32 cached_mpu_start_addr;
	u32 cached_mpu_total_size;
	u32 reserve_addr[12];
	u32 tail_pattern;
};

enum HIF_EX_STAGE {
	HIF_EX_INIT = 0, /* interrupt */
	HIF_EX_ACK, /* AP->MD */
	HIF_EX_INIT_DONE, /* polling */
	HIF_EX_CLEARQ_DONE, /* interrupt */
	HIF_EX_CLEARQ_ACK, /* AP->MD */
	HIF_EX_ALLQ_RESET, /* polling */
};

enum {
	P_CORE = 0,
	VOLTE_CORE,
};

enum {
	EXTERNAL_MODEM = 0,
	INTERNAL_MODEM = 1,
	MULTI_MD_MPU_SUPPORT = 2,
}; /* SHARE_MEMORY_SUPPORT */

/* runtime data format uses EEMCS's version, NOT the same with legacy CCCI */
struct modem_runtime {
	u32 Prefix;			 /* "CCIF" */
	u32 Platform_L;		 /* Hardware Platform String ex: "TK6516E0" */
	u32 Platform_H;
	u32 DriverVersion;	  /* 0x00000923 since W09.23 */
	u32 BootChannel;		/* Channel to ACK AP with boot ready */
	/* MD is booting. NORMAL_BOOT_ID or META_BOOT_ID */
	u32 BootingStartID;
#if 1 /* not using in EEMCS */
	u32 BootAttributes;	 /* Attributes passing from AP to MD Booting */
	/* MD response ID if boot successful and ready */
	u32 BootReadyID;
	u32 FileShareMemBase;
	u32 FileShareMemSize;
	u32 ExceShareMemBase;
	u32 ExceShareMemSize;
	u32 CCIFShareMemBase;
	u32 CCIFShareMemSize;
	u32 DHLShareMemBase; /* For DHL */
	u32 DHLShareMemSize;
	u32 MD1MD3ShareMemBase; /* For MD1 MD3 share memory */
	u32 MD1MD3ShareMemSize;
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

struct ccci_runtime_md_mem_ap_addr {
	u32 md_view_phy;
	u32 size;
	u32 ap_view_phy_lo32;
	u32 ap_view_phy_hi32;
};

enum {
	MD_FLIGHT_MODE_NONE = 0,
	MD_FLIGHT_MODE_ENTER = 1,
	MD_FLIGHT_MODE_LEAVE = 2
};/* FLIGHT_STAGE */

extern unsigned int is_cdma2000_enable(int md_id);

struct ccci_mem_layout *ccci_md_get_mem(int md_id);
struct ccci_smem_region *ccci_md_get_smem_by_user_id(int md_id,
	enum SMEM_USER_ID user_id);
void ccci_md_clear_smem(int md_id, int first_boot);
int ccci_md_start(unsigned char md_id);
int ccci_md_soft_start(unsigned char md_id, unsigned int sim_mode);
int ccci_md_send_runtime_data(unsigned char md_id);
int ccci_md_reset_pccif(unsigned char md_id);
void ccci_md_dump_info(unsigned char md_id, enum MODEM_DUMP_FLAG flag,
	void *buff, int length);
int ccci_md_pre_stop(unsigned char md_id, unsigned int stop_type);
int ccci_md_stop(unsigned char md_id, unsigned int stop_type);
int ccci_md_soft_stop(unsigned char md_id, unsigned int sim_mode);
int ccci_md_force_assert(unsigned char md_id, enum MD_FORCE_ASSERT_TYPE type,
	char *param, int len);
int ccci_md_prepare_runtime_data(unsigned char md_id, unsigned char *data,
	int length);
void ccci_md_exception_handshake(unsigned char md_id, int timeout);
int ccci_md_send_ccb_tx_notify(unsigned char md_id, int core_id);
int ccci_md_set_boot_data(unsigned char md_id, unsigned int data[], int len);
int ccci_md_pre_start(unsigned char md_id);
int ccci_md_post_start(unsigned char md_id);

struct ccci_modem_cfg {
	unsigned int load_type;
	unsigned int load_type_saving;
	unsigned int setting;
};

struct ccci_sim_setting {
	int sim_mode;
	int slot1_mode;		/* 0:CDMA 1:GSM 2:WCDMA 3:TDCDMA */
	int slot2_mode;		/* 0:CDMA 1:GSM 2:WCDMA 3:TDCDMA */
};

/*per modem data*/
struct ccci_per_md {
	unsigned int md_capability;
	unsigned int md_dbg_dump_flag;
	enum MD_BOOT_MODE md_boot_mode;
	char img_post_fix[IMG_POSTFIX_LEN];
	struct ccci_image_info img_info[IMG_NUM];
	unsigned int md_boot_data[16];
	unsigned int sim_type;
	struct ccci_modem_cfg config;
	unsigned int md_img_exist[MAX_IMG_NUM];
	unsigned int md_img_type_is_set;
	struct ccci_sim_setting sim_setting;
	int data_usb_bypass;
	int dtr_state; /* only for usb bypass */
	unsigned int is_in_ee_dump;

	unsigned long long latest_isr_time;
	unsigned long long latest_q0_isr_time;
	unsigned long long latest_q0_rx_time;
#ifdef CCCI_SKB_TRACE
	unsigned long long netif_rx_profile[8];
#endif
};
struct ccci_per_md *ccci_get_per_md_data(unsigned char md_id);

static inline int ccci_md_get_cap_by_id(int md_id)
{
	struct ccci_per_md *per_md_data = ccci_get_per_md_data(md_id);

	if (per_md_data == NULL)
		return -CCCI_ERR_MD_INDEX_NOT_FOUND;
	return per_md_data->md_capability;
}

struct ccci_runtime_feature *ccci_md_get_rt_feature_by_id(unsigned char md_id,
	u8 feature_id, u8 ap_query_md);

int ccci_md_parse_rt_feature(unsigned char md_id,
	struct ccci_runtime_feature *rt_feature, void *data, u32 data_len);

#endif
