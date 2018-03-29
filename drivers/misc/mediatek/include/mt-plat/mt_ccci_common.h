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

#ifndef __MT_CCCI_COMMON_H__
#define __MT_CCCI_COMMON_H__
#include <asm/io.h>
#include <asm/setup.h>
#include <linux/device.h>
#include <linux/skbuff.h>
/*
 * all code owned by CCCI should use modem index starts from ZERO
 */
typedef enum {
	MD_SYS1 = 0, /* MD SYS name counts from 1, but internal index counts from 0 */
	MD_SYS2,
	MD_SYS3,
	MD_SYS4,
	MD_SYS5 = 4,
	MAX_MD_NUM
} MD_SYS;

/* MD type defination */
typedef enum {
	md_type_invalid = 0,
	modem_2g = 1,
	modem_3g,
	modem_wg,
	modem_tg,
	modem_lwg,
	modem_ltg,
	modem_sglte,
	modem_ultg,
	modem_ulwg,
	modem_ulwtg,
	modem_ulwcg,
	modem_ulwctg,
	modem_ulttg,
	modem_ulfwg,
	modem_ulfwcg,
	modem_ulctg,
	modem_ultctg,
	modem_ultwg,
	modem_ultwcg,
	modem_ulftg,
	modem_ulfctg,
	MAX_IMG_NUM = modem_ulfctg /* this enum starts from 1 */
} MD_LOAD_TYPE;

/* MD logger configure file */
#define MD1_LOGGER_FILE_PATH "/data/mdlog/mdlog1_config"
#define MD2_LOGGER_FILE_PATH "/data/mdlog/mdlog2_config"

/* Image string and header */
/* image name/path */
#define MOEDM_IMAGE_NAME			"modem.img"
#define DSP_IMAGE_NAME				"DSP_ROM"
#define CONFIG_MODEM_FIRMWARE_PATH		"/etc/firmware/"
#define CONFIG_MODEM_FIRMWARE_CIP_PATH	"/custom/etc/firmware/"
#define IMG_ERR_STR_LEN				 64

/* image header constants */
#define MD_HEADER_MAGIC_NO "CHECK_HEADER"

#define DEBUG_STR		"Debug"
#define RELEASE_STR		"Release"
#define INVALID_STR		"INVALID"

struct ccci_header {
	u32 data[2]; /* do NOT assump data[1] is data length in Rx */
/* #ifdef FEATURE_SEQ_CHECK_EN */
	u16 channel:16;
	u16 seq_num:15;
	u16 assert_bit:1;
/* #else */
/* u32 channel; */
/* #endif */
	u32 reserved;
} __packed; /* not necessary, but it's a good gesture, :) */

struct md_check_header {
	unsigned char check_header[12];  /* magic number is "CHECK_HEADER"*/
	unsigned int  header_verno;	  /* header structure version number */
	unsigned int  product_ver;	   /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int  image_type;		/* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned char platform[16];	  /* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64];	/* build time string */
	unsigned char build_ver[64];	 /* project version, ex:11A_MD.W11.28 */

	unsigned char bind_sys_id;	   /* bind to md sys id, MD SYS1: 1, MD SYS2: 2 */
	unsigned char ext_attr;		  /* no shrink: 0, shrink: 1*/
	unsigned char reserved[2];	   /* for reserved */

	unsigned int  mem_size;		  /* md ROM/RAM image size requested by md */
	unsigned int  md_img_size;	   /* md image size, exclude head size*/
	unsigned int  reserved_info;	 /* for reserved */
	unsigned int  size;			  /* the size of this structure */
} __packed;

struct _md_regin_info {
	unsigned int region_offset;
	unsigned int region_size;
};

/* ======================= */
/* index id region_info                        */
/* ----------------------------- */
enum {
	/* MPU_REGION_INFO_ID_SEC_OS,
	MPU_REGION_INFO_ID_ATF,
	MPU_REGION_INFO_ID_SCP_OS,
	MPU_REGION_INFO_ID_MD1_SEC_SMEM,
	MPU_REGION_INFO_ID_MD2_SEC_SMEM,
	MPU_REGION_INFO_ID_MD1_SMEM,
	MPU_REGION_INFO_ID_MD2_SMEM,
	MPU_REGION_INFO_ID_MD1_MD2_SMEM, */
	MD_SET_REGION_MD1_ROM_DSP = 0,
	MD_SET_REGION_MD1_MCURW_HWRO,
	MD_SET_REGION_MD1_MCURO_HWRW,
	MD_SET_REGION_MD1_MCURW_HWRW,/* old dsp region */
	MD_SET_REGION_MD1_RW = 4,
	/* MPU_REGION_INFO_ID_MD2_ROM,
	MPU_REGION_INFO_ID_MD2_RW,
	MPU_REGION_INFO_ID_AP,
	MPU_REGION_INFO_ID_WIFI_EMI_FW,
	MPU_REGION_INFO_ID_WMT, */
	MPU_REGION_INFO_ID_LAST = MD_SET_REGION_MD1_RW,
	MPU_REGION_INFO_ID_TOTAL_NUM,
};

/* ====================== */
/* domain info                                 */
/* ---------------------------- */
enum{
	MPU_DOMAIN_ID_AP = 0,
	MPU_DOMAIN_ID_MD = 1,
	MPU_DOMAIN_ID_MD3 = 5,
	MPU_DOMAIN_ID_MDHW = 7,
	MPU_DOMAIN_ID_TOTAL_NUM,
};

/* ====================== */
/* index id domain_attr                     */
/* ---------------------------- */
enum{
	MPU_DOMAIN_INFO_ID_MD1 = 0,
	MPU_DOMAIN_INFO_ID_MD3,
	MPU_DOMAIN_INFO_ID_MDHW,
	MPU_DOMAIN_INFO_ID_LAST = MPU_DOMAIN_INFO_ID_MDHW,
	MPU_DOMAIN_INFO_ID_TOTAL_NUM,
};

/* ================================================================================= */
/* CCCI Error number defination */
/* ================================================================================= */
/* CCCI error number region */
#define CCCI_ERR_MODULE_INIT_START_ID   (0)
#define CCCI_ERR_COMMON_REGION_START_ID	(100)
#define CCCI_ERR_CCIF_REGION_START_ID	(200)
#define CCCI_ERR_CCCI_REGION_START_ID	(300)
#define CCCI_ERR_LOAD_IMG_START_ID		(400)

/* CCCI error number */
#define CCCI_ERR_MODULE_INIT_OK					 (CCCI_ERR_MODULE_INIT_START_ID+0)
#define CCCI_ERR_INIT_DEV_NODE_FAIL				 (CCCI_ERR_MODULE_INIT_START_ID+1)
#define CCCI_ERR_INIT_PLATFORM_FAIL				 (CCCI_ERR_MODULE_INIT_START_ID+2)
#define CCCI_ERR_MK_DEV_NODE_FAIL				   (CCCI_ERR_MODULE_INIT_START_ID+3)
#define CCCI_ERR_INIT_LOGIC_LAYER_FAIL			  (CCCI_ERR_MODULE_INIT_START_ID+4)
#define CCCI_ERR_INIT_MD_CTRL_FAIL				  (CCCI_ERR_MODULE_INIT_START_ID+5)
#define CCCI_ERR_INIT_CHAR_DEV_FAIL				 (CCCI_ERR_MODULE_INIT_START_ID+6)
#define CCCI_ERR_INIT_TTY_FAIL					  (CCCI_ERR_MODULE_INIT_START_ID+7)
#define CCCI_ERR_INIT_IPC_FAIL					  (CCCI_ERR_MODULE_INIT_START_ID+8)
#define CCCI_ERR_INIT_RPC_FAIL					  (CCCI_ERR_MODULE_INIT_START_ID+9)
#define CCCI_ERR_INIT_FS_FAIL					   (CCCI_ERR_MODULE_INIT_START_ID+10)
#define CCCI_ERR_INIT_CCMNI_FAIL					(CCCI_ERR_MODULE_INIT_START_ID+11)
#define CCCI_ERR_INIT_VIR_CHAR_FAIL				 (CCCI_ERR_MODULE_INIT_START_ID+12)

/* ---- Common */
#define CCCI_ERR_FATAL_ERR							(CCCI_ERR_COMMON_REGION_START_ID+0)
#define CCCI_ERR_ASSERT_ERR							(CCCI_ERR_COMMON_REGION_START_ID+1)
#define CCCI_ERR_MD_IN_RESET						(CCCI_ERR_COMMON_REGION_START_ID+2)
#define CCCI_ERR_RESET_NOT_READY					(CCCI_ERR_COMMON_REGION_START_ID+3)
#define CCCI_ERR_GET_MEM_FAIL						(CCCI_ERR_COMMON_REGION_START_ID+4)
#define CCCI_ERR_GET_SMEM_SETTING_FAIL				(CCCI_ERR_COMMON_REGION_START_ID+5)
#define CCCI_ERR_INVALID_PARAM						(CCCI_ERR_COMMON_REGION_START_ID+6)
#define CCCI_ERR_LARGE_THAN_BUF_SIZE				(CCCI_ERR_COMMON_REGION_START_ID+7)
#define CCCI_ERR_GET_MEM_LAYOUT_FAIL				(CCCI_ERR_COMMON_REGION_START_ID+8)
#define CCCI_ERR_MEM_CHECK_FAIL						(CCCI_ERR_COMMON_REGION_START_ID+9)
#define CCCI_IPO_H_RESTORE_FAIL						(CCCI_ERR_COMMON_REGION_START_ID+10)

/* ---- CCIF */
#define CCCI_ERR_CCIF_NOT_READY						(CCCI_ERR_CCIF_REGION_START_ID+0)
#define CCCI_ERR_CCIF_CALL_BACK_HAS_REGISTERED		(CCCI_ERR_CCIF_REGION_START_ID+1)
#define CCCI_ERR_CCIF_GET_NULL_POINTER				(CCCI_ERR_CCIF_REGION_START_ID+2)
#define CCCI_ERR_CCIF_UN_SUPPORT					(CCCI_ERR_CCIF_REGION_START_ID+3)
#define CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL			(CCCI_ERR_CCIF_REGION_START_ID+4)
#define CCCI_ERR_CCIF_INVALID_RUNTIME_LEN			(CCCI_ERR_CCIF_REGION_START_ID+5)
#define CCCI_ERR_CCIF_INVALID_MD_SYS_ID				(CCCI_ERR_CCIF_REGION_START_ID+6)
#define CCCI_ERR_CCIF_GET_HW_INFO_FAIL				(CCCI_ERR_CCIF_REGION_START_ID+9)

/* ---- CCCI */
#define CCCI_ERR_INVALID_LOGIC_CHANNEL_ID			(CCCI_ERR_CCCI_REGION_START_ID+0)
#define CCCI_ERR_PUSH_RX_DATA_TO_TX_CHANNEL			(CCCI_ERR_CCCI_REGION_START_ID+1)
#define CCCI_ERR_REG_CALL_BACK_FOR_TX_CHANNEL		(CCCI_ERR_CCCI_REGION_START_ID+2)
#define CCCI_ERR_LOGIC_CH_HAS_REGISTERED			(CCCI_ERR_CCCI_REGION_START_ID+3)
#define CCCI_ERR_MD_NOT_READY						(CCCI_ERR_CCCI_REGION_START_ID+4)
#define CCCI_ERR_ALLOCATE_MEMORY_FAIL				(CCCI_ERR_CCCI_REGION_START_ID+5)
#define CCCI_ERR_CREATE_CCIF_INSTANCE_FAIL			(CCCI_ERR_CCCI_REGION_START_ID+6)
#define CCCI_ERR_REPEAT_CHANNEL_ID					(CCCI_ERR_CCCI_REGION_START_ID+7)
#define CCCI_ERR_KFIFO_IS_NOT_READY					(CCCI_ERR_CCCI_REGION_START_ID+8)
#define CCCI_ERR_GET_NULL_POINTER					(CCCI_ERR_CCCI_REGION_START_ID+9)
#define CCCI_ERR_GET_RX_DATA_FROM_TX_CHANNEL		(CCCI_ERR_CCCI_REGION_START_ID+10)
#define CCCI_ERR_CHANNEL_NUM_MIS_MATCH				(CCCI_ERR_CCCI_REGION_START_ID+11)
#define CCCI_ERR_START_ADDR_NOT_4BYTES_ALIGN		(CCCI_ERR_CCCI_REGION_START_ID+12)
#define CCCI_ERR_NOT_DIVISIBLE_BY_4					(CCCI_ERR_CCCI_REGION_START_ID+13)
#define CCCI_ERR_MD_AT_EXCEPTION					(CCCI_ERR_CCCI_REGION_START_ID+14)
#define CCCI_ERR_MD_CB_HAS_REGISTER					(CCCI_ERR_CCCI_REGION_START_ID+15)
#define CCCI_ERR_MD_INDEX_NOT_FOUND					(CCCI_ERR_CCCI_REGION_START_ID+16)
#define CCCI_ERR_DROP_PACKET						(CCCI_ERR_CCCI_REGION_START_ID+17)
#define CCCI_ERR_PORT_RX_FULL						(CCCI_ERR_CCCI_REGION_START_ID+18)
#define CCCI_ERR_SYSFS_NOT_READY					(CCCI_ERR_CCCI_REGION_START_ID+19)
#define CCCI_ERR_IPC_ID_ERROR						(CCCI_ERR_CCCI_REGION_START_ID+20)
#define CCCI_ERR_FUNC_ID_ERROR						(CCCI_ERR_CCCI_REGION_START_ID+21)
#define CCCI_ERR_INVALID_QUEUE_INDEX				(CCCI_ERR_CCCI_REGION_START_ID+21)
#define CCCI_ERR_HIF_NOT_POWER_ON					(CCCI_ERR_CCCI_REGION_START_ID+22)

/* ---- Load image error */
#define CCCI_ERR_LOAD_IMG_NOMEM						(CCCI_ERR_LOAD_IMG_START_ID+0)
#define CCCI_ERR_LOAD_IMG_FILE_OPEN					(CCCI_ERR_LOAD_IMG_START_ID+1)
#define CCCI_ERR_LOAD_IMG_FILE_READ					(CCCI_ERR_LOAD_IMG_START_ID+2)
#define CCCI_ERR_LOAD_IMG_KERN_READ					(CCCI_ERR_LOAD_IMG_START_ID+3)
#define CCCI_ERR_LOAD_IMG_NO_ADDR					(CCCI_ERR_LOAD_IMG_START_ID+4)
#define CCCI_ERR_LOAD_IMG_NO_FIRST_BOOT				(CCCI_ERR_LOAD_IMG_START_ID+5)
#define CCCI_ERR_LOAD_IMG_LOAD_FIRM					(CCCI_ERR_LOAD_IMG_START_ID+6)
#define CCCI_ERR_LOAD_IMG_FIRM_NULL					(CCCI_ERR_LOAD_IMG_START_ID+7)
#define CCCI_ERR_LOAD_IMG_CHECK_HEAD				(CCCI_ERR_LOAD_IMG_START_ID+8)
#define CCCI_ERR_LOAD_IMG_SIGN_FAIL					(CCCI_ERR_LOAD_IMG_START_ID+9)
#define CCCI_ERR_LOAD_IMG_CIPHER_FAIL				(CCCI_ERR_LOAD_IMG_START_ID+10)
#define CCCI_ERR_LOAD_IMG_MD_CHECK					(CCCI_ERR_LOAD_IMG_START_ID+11)
#define CCCI_ERR_LOAD_IMG_DSP_CHECK					(CCCI_ERR_LOAD_IMG_START_ID+12)
#define CCCI_ERR_LOAD_IMG_ABNORAL_SIZE				(CCCI_ERR_LOAD_IMG_START_ID+13)
#define CCCI_ERR_LOAD_IMG_NOT_FOUND					(CCCI_ERR_LOAD_IMG_START_ID+13)

/* export to other kernel modules, better not let other module include ECCCI header directly (except IPC...) */
typedef enum {
	MD_STATE_INVALID = 0,
	MD_STATE_BOOTING = 1,
	MD_STATE_READY = 2,
	MD_STATE_EXCEPTION = 3
} MD_STATE_FOR_USER;

typedef enum {
	ID_GET_MD_WAKEUP_SRC = 0,   /* for SPM */
	ID_CCCI_DORMANCY = 1,	   /* abandoned */
	ID_LOCK_MD_SLEEP = 2,	   /* abandoned */
	ID_ACK_MD_SLEEP = 3,		/* abandoned */
	ID_SSW_SWITCH_MODE = 4,	 /* abandoned */
	ID_SET_MD_TX_LEVEL = 5,	 /* abandoned */
	ID_GET_TXPOWER = 6,		 /* for thermal */
	ID_IPO_H_RESTORE_CB = 7,	/* abandoned */
	ID_FORCE_MD_ASSERT = 8,	 /* abandoned */
	ID_PAUSE_LTE = 9,		/* for DVFS */
	ID_STORE_SIM_SWITCH_MODE = 10,
	ID_GET_SIM_SWITCH_MODE = 11,
	ID_GET_MD_STATE = 12,		/* for DVFS */
	ID_THROTTLING_CFG = 13,		/* For MD SW throughput throttling */
	ID_RESET_MD = 14,			/* for SVLTE MD3 reset MD1 */
	ID_DUMP_MD_REG = 15,
	ID_DUMP_MD_SLEEP_MODE = 16, /* for dump MD debug info from SMEM when AP sleep */
	ID_PMIC_INTR = 17, /* for PMIC to notify MD buck over current, called from kernel thread context */
	ID_STOP_MD = 18,
	ID_START_MD = 19,
	ID_UPDATE_MD_BOOT_MODE = 20,
	ID_MD_MPU_ASSERT = 21,

	ID_UPDATE_TX_POWER = 100,   /* for SWTP */

} KERN_FUNC_ID;

enum {
	/*bit0-bit15: for modem capability related with ccci or ccci&ccmni driver*/
	MODEM_CAP_NAPI = (1<<0),
	MODEM_CAP_TXBUSY_STOP = (1<<1),
	MODEM_CAP_SGIO = (1<<2),
	/*bit16-bit31: for modem capability only related with ccmni driver*/
	MODEM_CAP_CCMNI_DISABLE = (1<<16),
	MODEM_CAP_DATA_ACK_DVD = (1<<17),
	MODEM_CAP_CCMNI_SEQNO = (1<<18),
	MODEM_CAP_CCMNI_IRAT = (1<<19),
	MODEM_CAP_WORLD_PHONE = (1<<20),
	MODEM_CAP_CCMNI_MQ = (1<<21), /* it must depend on DATA ACK DEVIDE feature */
};

typedef enum {
	INVALID = 0, /* no traffic */
	GATED, /* broadcast by modem driver, no traffic */
	BOOT_WAITING_FOR_HS1, /* broadcast by modem driver */
	BOOT_WAITING_FOR_HS2, /* broadcast by modem driver */
	READY, /* broadcast by port_kernel */
	EXCEPTION, /* broadcast by port_kernel */
	RESET, /* broadcast by modem driver, no traffic */
	WAITING_TO_STOP,

	RX_IRQ, /* broadcast by modem driver, illegal for md->md_state, only for NAPI! */
	RX_FLUSH, /* broadcast by modem driver, illegal for md->md_state, only for GRO! */
	TX_IRQ, /* broadcast by modem driver, illegal for md->md_state, only for network! */
	TX_FULL, /* broadcast by modem driver, illegal for md->md_state, only for network! */
} MD_STATE; /* for CCCI internal */

/* ================================================================================= */
/* Image type and header defination part */
/* ================================================================================= */
typedef enum {
	IMG_MD = 0,
	IMG_DSP,
	IMG_ARMV7,
	IMG_NUM,
} MD_IMG_TYPE;

typedef enum{
	INVALID_VARSION = 0,
	DEBUG_VERSION,
	RELEASE_VERSION
} PRODUCT_VER_TYPE;

#define IMG_NAME_LEN 32
#define IMG_POSTFIX_LEN 16
#define IMG_PATH_LEN 64

struct IMG_CHECK_INFO {
	char *product_ver;	/* debug/release/invalid */
	char *image_type;	/*2G/3G/invalid*/
	char *platform;		/* MT6573_S00(MT6573E1) or MT6573_S01(MT6573E2) */
	char *build_time;	/* build time string */
	char *build_ver;	/* project version, ex:11A_MD.W11.28 */
	unsigned int mem_size; /*md rom+ram mem size*/
	unsigned int md_img_size; /*modem image actual size, exclude head size*/
	PRODUCT_VER_TYPE version;
	unsigned int header_verno;  /* header structure version number */
};

struct IMG_REGION_INFO {
	unsigned int  region_num;        /* total region number */
	struct _md_regin_info region_info[8];    /* max support 8 regions */
	unsigned int  domain_attr[4];    /* max support 4 domain settings, each region has 4 control bits*/
};

struct ccci_image_info {
	MD_IMG_TYPE type;
	char file_name[IMG_PATH_LEN];
	phys_addr_t address; /* phy memory address to load this image */
	unsigned int size; /* image size without signature, cipher and check header, read form check header */
	unsigned int offset; /* signature and cipher header */
	unsigned int tail_length; /* signature tail */
	unsigned int dsp_offset;
	unsigned int dsp_size;
	unsigned int arm7_offset;
	unsigned int arm7_size;
	char *ap_platform;
	struct IMG_CHECK_INFO img_info; /* read from MD image header */
	struct IMG_CHECK_INFO ap_info; /* get from AP side configuration */
	struct IMG_REGION_INFO rmpu_info;  /* refion pinfo for RMPU setting */
};

typedef int (*get_status_func_t)(int, char*, int);
typedef int (*boot_md_func_t)(int);

typedef enum {
	SMEM_USER_RAW_DBM = 0,
	SMEM_USER_CCB_DHL,
	SMEM_USER_RAW_DHL,
	SMEM_USER_RAW_NETD,
	SMEM_USER_RAW_USB,
	SMEM_USER_MAX,
} SMEM_USER_ID;

typedef enum {
	ID_GET_FDD_THERMAL_DATA = 0,
	ID_GET_TDD_THERMAL_DATA,
} SYS_CB_ID;

typedef int (*ccci_sys_cb_func_t)(int, int);
typedef struct{
	SYS_CB_ID		id;
	ccci_sys_cb_func_t	func;
} ccci_sys_cb_func_info_t;

#define MAX_KERN_API 24 /* 20 */

/* ============================================================================================== */
/* Export API */
/* ============================================================================================== */
int ccci_get_fo_setting(char item[], unsigned int *val); /* Export by ccci util */
void ccci_md_mem_reserve(void); /* Export by ccci util */
unsigned int get_modem_is_enabled(int md_id); /* Export by ccci util */
unsigned int ccci_get_modem_nr(void); /* Export by ccci util */
int ccci_init_security(void); /* Export by ccci util */
int ccci_sysfs_add_modem(int md_id, void *kobj, void *ktype,
					get_status_func_t, boot_md_func_t); /* Export by ccci util */
int get_modem_support_cap(int md_id); /* Export by ccci util */
int set_modem_support_cap(int md_id, int new_val); /* Export by ccci util */
char *ccci_get_md_info_str(int md_id); /* Export by ccci util */
void get_md_postfix(int md_id, char k[], char buf[], char buf_ex[]); /* Export by ccci util */
void update_ccci_port_ver(unsigned int new_ver); /* Export by ccci util */
/* Export by ccci util */
int ccci_load_firmware(int md_id, void *img_inf, char img_err_str[], char post_fix[], struct device *dev);
int get_md_resv_mem_info(int md_id, phys_addr_t *r_rw_base, unsigned int *r_rw_size,
					phys_addr_t *srw_base, unsigned int *srw_size); /* Export by ccci util */
int get_md1_md3_resv_smem_info(int md_id, phys_addr_t *rw_base, unsigned int *rw_size);
/* used for throttling feature - start */
unsigned long ccci_get_md_boot_count(int md_id);
/* used for throttling feature - end */

int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf, unsigned int len);
int register_ccci_sys_call_back(int md_id, unsigned int id, ccci_sys_cb_func_t func);
int switch_sim_mode(int id, char *buf, unsigned int len);
unsigned int get_sim_switch_type(void);

#ifdef CONFIG_MTK_ECCCI_C2K
/* for c2k usb bypass */
int ccci_c2k_rawbulk_intercept(int ch_id, unsigned int interception);
int ccci_c2k_buffer_push(int ch_id, void *buf, int count);
int modem_dtr_set(int on, int low_latency);
int modem_dcd_state(void);
#endif
/* CLib for modem get ap time */
void notify_time_update(void);
int wait_time_update_notify(void);
/*cb API for system power off*/
void ccci_power_off(void);
/* Ubin API */
int md_capability(int md_id, int wm_id, int curr_md_type);
int get_md_wm_id_map(int ap_wm_id);
/* LK load modem */
int modem_run_env_ready(int md_id);
int get_lk_load_md_info(char buf[], int size);
int get_md_type_from_lk(int md_id);
int get_raw_check_hdr(int md_id, char buf[], int size);
int ccci_get_md_check_hdr_inf(int md_id, void *img_inf, char post_fix[]);
int get_md_img_raw_size(int md_id);
void clear_meta_1st_boot_arg(int md_id);
/* for kernel share memory user */
void __iomem *get_smem_start_addr(int md_id, SMEM_USER_ID user_id, int *size_o);

/* CCCI dump */
#define CCCI_DUMP_TIME_FLAG		(1<<0)
#define CCCI_DUMP_CLR_BUF_FLAG	(1<<1)
#define CCCI_DUMP_CURR_FLAG		(1<<2)
enum {
	CCCI_DUMP_INIT = 0,
	CCCI_DUMP_BOOTUP,
	CCCI_DUMP_NORMAL,
	CCCI_DUMP_REPEAT,
	CCCI_DUMP_MEM_DUMP,
	CCCI_DUMP_HISTORY,
	CCCI_DUMP_MAX,
};
void ccci_util_mem_dump(int md_id, int buf_type, void *start_addr, int len);
void ccci_util_cmpt_mem_dump(int md_id, int buf_type, void *start_addr, int len);
int ccci_dump_write(int md_id, int buf_type, unsigned int flag, const char *fmt, ...);
int ccci_log_write(const char *fmt, ...);
int ccci_log_write_raw(unsigned int flags, const char *fmt, ...);
int ccci_event_log_cpy(char buf[], int size);
int ccci_event_log(const char *fmt, ...);
int ccmni_send_mbim_skb(int md_id, struct sk_buff *skb);
void ccmni_update_mbim_interface(int md_id, int id);

/* MPU setting */
typedef struct _mpu_cfg {
	unsigned int start;
	unsigned int end;
	int region;
	unsigned int permission;
	int relate_region; /* Using same behavior and setting */
} mpu_cfg_t;
mpu_cfg_t *get_mpu_region_cfg_info(int region_id);
int ccci_get_opt_val(char *opt_name);

/* Rat configure relate */
int ccci_get_rat_str_from_drv(int md_id, char rat_str[], int size);
void ccci_set_rat_str_to_drv(int md_id, char rat_str[]);
unsigned int get_wm_bitmap_for_ubin(void); /* Universal bin */
void update_rat_bit_map_to_drv(int md_id, unsigned int val);
int get_md_img_type(int md_id);

#endif
