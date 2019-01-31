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

#ifndef __MT_CCCI_COMMON_H__
#define __MT_CCCI_COMMON_H__
#include <linux/io.h>
#include <asm/setup.h>
#include <linux/device.h>
#include <linux/skbuff.h>
/*
 * all code owned by CCCI should use modem index starts from ZERO
 */
enum MD_SYS {
	/* MD SYS name counts from 1,
	 * but internal index counts from 0
	 */
	MD_SYS1 = 0,
	MD_SYS2,
	MD_SYS3,
	MD_SYS4,
	MD_SYS5 = 4,
	MAX_MD_NUM
};

/* MD type defination */
enum MD_LOAD_TYPE {
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
};

/* MD logger configure file */
#define MD1_LOGGER_FILE_PATH "/data/mdlog/mdlog1_config"
#define MD2_LOGGER_FILE_PATH "/data/mdlog/mdlog2_config"

/* Image string and header */
/* image name/path */
#define MOEDM_IMAGE_NAME		"modem.img"
#define DSP_IMAGE_NAME			"DSP_ROM"
#define CONFIG_MODEM_FIRMWARE_PATH	"/etc/firmware/"
#define CONFIG_MODEM_FIRMWARE_CIP_PATH	"/custom/etc/firmware/"
#define IMG_ERR_STR_LEN			(64)

/* image header constants */
#define MD_HEADER_MAGIC_NO	"CHECK_HEADER"
#define DEBUG_STR		"Debug"
#define RELEASE_STR		"Release"
#define INVALID_STR		"INVALID"

struct ccci_header {
	/* do NOT assume data[1] is data length in Rx */
	u32 data[2];
	u16 channel:16;
	u16 seq_num:15;
	u16 assert_bit:1;
	u32 reserved;
} __packed;

struct lhif_header {
	u16 pdcp_count;
	u8 flow:4;
	u8 f:3;
	u8 reserved:1;
	u8 netif:5;
	u8 nw_type:3;
} __packed;

struct md_check_header {
	unsigned char check_header[12];	/* magic number is "CHECK_HEADER"*/
	unsigned int  header_verno;	/* header structure version number */
	/* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int  product_ver;

	/* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned int  image_type;
	unsigned char platform[16];	/* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64];	/* build time string */
	unsigned char build_ver[64];	/* project version, ex:11A_MD.W11.28 */

	/* bind to md sys id, MD SYS1: 1, MD SYS2: 2 */
	unsigned char bind_sys_id;
	unsigned char ext_attr;		/* no shrink: 0, shrink: 1*/
	unsigned char reserved[2];	/* for reserved */

	/* md ROM/RAM image size requested by md */
	unsigned int  mem_size;
	unsigned int  md_img_size;	/* md image size, exclude head size*/
	unsigned int  reserved_info;	/* for reserved */
	unsigned int  size;		/* the size of this structure */
} __packed;

struct _md_regin_info {
	unsigned int region_offset;
	unsigned int region_size;
};

/* ======================= */
/* index id region_info    */
/* ----------------------- */
enum {
	MD_SET_REGION_MD1_ROM_DSP = 0,
	MD_SET_REGION_MD1_MCURW_HWRO,
	MD_SET_REGION_MD1_MCURO_HWRW,
	MD_SET_REGION_MD1_MCURW_HWRW,/* old dsp region */
	MD_SET_REGION_MD1_RW = 4,
	MPU_REGION_INFO_ID_LAST = MD_SET_REGION_MD1_RW,
	MPU_REGION_INFO_ID_TOTAL_NUM,
};

/* ====================== */
/* domain info            */
/* ---------------------- */
enum{
	MPU_DOMAIN_ID_AP = 0,
	MPU_DOMAIN_ID_MD = 1,
	MPU_DOMAIN_ID_MD3 = 5,
	MPU_DOMAIN_ID_MDHW = 7,
	MPU_DOMAIN_ID_TOTAL_NUM,
};

/* ====================== */
/* index id domain_attr   */
/* -----------------------*/
enum{
	MPU_DOMAIN_INFO_ID_MD1 = 0,
	MPU_DOMAIN_INFO_ID_MD3,
	MPU_DOMAIN_INFO_ID_MDHW,
	MPU_DOMAIN_INFO_ID_LAST = MPU_DOMAIN_INFO_ID_MDHW,
	MPU_DOMAIN_INFO_ID_TOTAL_NUM,
};

/* ============================================================= */
/* CCCI Error number defination */
/* ============================================================= */
/* CCCI error number region */
#define CCCI_ERR_MODULE_INIT_START_ID   (0)
#define CCCI_ERR_COMMON_REGION_START_ID	(100)
#define CCCI_ERR_CCIF_REGION_START_ID	(200)
#define CCCI_ERR_CCCI_REGION_START_ID	(300)
#define CCCI_ERR_LOAD_IMG_START_ID	(400)

/* CCCI error number */
#define CCCI_ERR_MODULE_INIT_O \
	(CCCI_ERR_MODULE_INIT_START_ID+0)
#define CCCI_ERR_INIT_DEV_NODE_FAIL \
	(CCCI_ERR_MODULE_INIT_START_ID+1)
#define CCCI_ERR_INIT_PLATFORM_FAIL \
	(CCCI_ERR_MODULE_INIT_START_ID+2)
#define CCCI_ERR_MK_DEV_NODE_FAIL \
	(CCCI_ERR_MODULE_INIT_START_ID+3)
#define CCCI_ERR_INIT_LOGIC_LAYER_FAIL \
	(CCCI_ERR_MODULE_INIT_START_ID+4)
#define CCCI_ERR_INIT_MD_CTRL_FAIL \
	(CCCI_ERR_MODULE_INIT_START_ID+5)
#define CCCI_ERR_INIT_CHAR_DEV_FAIL \
	(CCCI_ERR_MODULE_INIT_START_ID+6)
#define CCCI_ERR_INIT_TTY_FAIL \
	(CCCI_ERR_MODULE_INIT_START_ID+7)
#define CCCI_ERR_INIT_IPC_FAIL \
	(CCCI_ERR_MODULE_INIT_START_ID+8)
#define CCCI_ERR_INIT_RPC_FAIL \
	(CCCI_ERR_MODULE_INIT_START_ID+9)
#define CCCI_ERR_INIT_FS_FAIL \
	(CCCI_ERR_MODULE_INIT_START_ID+10)
#define CCCI_ERR_INIT_CCMNI_FAIL \
	(CCCI_ERR_MODULE_INIT_START_ID+11)
#define CCCI_ERR_INIT_VIR_CHAR_FAIL \
	(CCCI_ERR_MODULE_INIT_START_ID+12)

/* ---- Common */
#define CCCI_ERR_FATAL_ERR \
	(CCCI_ERR_COMMON_REGION_START_ID+0)
#define CCCI_ERR_ASSERT_ERR \
	(CCCI_ERR_COMMON_REGION_START_ID+1)
#define CCCI_ERR_MD_IN_RESET \
	(CCCI_ERR_COMMON_REGION_START_ID+2)
#define CCCI_ERR_RESET_NOT_READY \
	(CCCI_ERR_COMMON_REGION_START_ID+3)
#define CCCI_ERR_GET_MEM_FAIL \
	(CCCI_ERR_COMMON_REGION_START_ID+4)
#define CCCI_ERR_GET_SMEM_SETTING_FAIL \
	(CCCI_ERR_COMMON_REGION_START_ID+5)
#define CCCI_ERR_INVALID_PARAM \
	(CCCI_ERR_COMMON_REGION_START_ID+6)
#define CCCI_ERR_LARGE_THAN_BUF_SIZE \
	(CCCI_ERR_COMMON_REGION_START_ID+7)
#define CCCI_ERR_GET_MEM_LAYOUT_FAIL \
	(CCCI_ERR_COMMON_REGION_START_ID+8)
#define CCCI_ERR_MEM_CHECK_FAIL \
	(CCCI_ERR_COMMON_REGION_START_ID+9)
#define CCCI_IPO_H_RESTORE_FAIL \
	(CCCI_ERR_COMMON_REGION_START_ID+10)

/* ---- CCIF */
#define CCCI_ERR_CCIF_NOT_READY \
	(CCCI_ERR_CCIF_REGION_START_ID+0)
#define CCCI_ERR_CCIF_CALL_BACK_HAS_REGISTERED \
	(CCCI_ERR_CCIF_REGION_START_ID+1)
#define CCCI_ERR_CCIF_GET_NULL_POINTER \
	(CCCI_ERR_CCIF_REGION_START_ID+2)
#define CCCI_ERR_CCIF_UN_SUPPORT \
	(CCCI_ERR_CCIF_REGION_START_ID+3)
#define CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL \
	(CCCI_ERR_CCIF_REGION_START_ID+4)
#define CCCI_ERR_CCIF_INVALID_RUNTIME_LEN \
	(CCCI_ERR_CCIF_REGION_START_ID+5)
#define CCCI_ERR_CCIF_INVALID_MD_SYS_ID \
	(CCCI_ERR_CCIF_REGION_START_ID+6)
#define CCCI_ERR_CCIF_GET_HW_INFO_FAIL \
	(CCCI_ERR_CCIF_REGION_START_ID+9)

/* ---- CCCI */
#define CCCI_ERR_INVALID_LOGIC_CHANNEL_ID \
	(CCCI_ERR_CCCI_REGION_START_ID+0)
#define CCCI_ERR_PUSH_RX_DATA_TO_TX_CHANNEL \
	(CCCI_ERR_CCCI_REGION_START_ID+1)
#define CCCI_ERR_REG_CALL_BACK_FOR_TX_CHANNEL \
	(CCCI_ERR_CCCI_REGION_START_ID+2)
#define CCCI_ERR_LOGIC_CH_HAS_REGISTERED \
	(CCCI_ERR_CCCI_REGION_START_ID+3)
#define CCCI_ERR_MD_NOT_READY \
	(CCCI_ERR_CCCI_REGION_START_ID+4)
#define CCCI_ERR_ALLOCATE_MEMORY_FAIL \
	(CCCI_ERR_CCCI_REGION_START_ID+5)
#define CCCI_ERR_CREATE_CCIF_INSTANCE_FAIL \
	(CCCI_ERR_CCCI_REGION_START_ID+6)
#define CCCI_ERR_REPEAT_CHANNEL_ID \
	(CCCI_ERR_CCCI_REGION_START_ID+7)
#define CCCI_ERR_KFIFO_IS_NOT_READY \
	(CCCI_ERR_CCCI_REGION_START_ID+8)
#define CCCI_ERR_GET_NULL_POINTER \
	(CCCI_ERR_CCCI_REGION_START_ID+9)
#define CCCI_ERR_GET_RX_DATA_FROM_TX_CHANNEL \
	(CCCI_ERR_CCCI_REGION_START_ID+10)
#define CCCI_ERR_CHANNEL_NUM_MIS_MATCH \
	(CCCI_ERR_CCCI_REGION_START_ID+11)
#define CCCI_ERR_START_ADDR_NOT_4BYTES_ALIGN \
	(CCCI_ERR_CCCI_REGION_START_ID+12)
#define CCCI_ERR_NOT_DIVISIBLE_BY_4 \
	(CCCI_ERR_CCCI_REGION_START_ID+13)
#define CCCI_ERR_MD_AT_EXCEPTION \
	(CCCI_ERR_CCCI_REGION_START_ID+14)
#define CCCI_ERR_MD_CB_HAS_REGISTER \
	(CCCI_ERR_CCCI_REGION_START_ID+15)
#define CCCI_ERR_MD_INDEX_NOT_FOUND \
	(CCCI_ERR_CCCI_REGION_START_ID+16)
#define CCCI_ERR_DROP_PACKET \
	(CCCI_ERR_CCCI_REGION_START_ID+17)
#define CCCI_ERR_PORT_RX_FULL \
	(CCCI_ERR_CCCI_REGION_START_ID+18)
#define CCCI_ERR_SYSFS_NOT_READY \
	(CCCI_ERR_CCCI_REGION_START_ID+19)
#define CCCI_ERR_IPC_ID_ERROR \
	(CCCI_ERR_CCCI_REGION_START_ID+20)
#define CCCI_ERR_FUNC_ID_ERROR \
	(CCCI_ERR_CCCI_REGION_START_ID+21)
#define CCCI_ERR_INVALID_QUEUE_INDEX \
	(CCCI_ERR_CCCI_REGION_START_ID+21)
#define CCCI_ERR_HIF_NOT_POWER_ON \
	(CCCI_ERR_CCCI_REGION_START_ID+22)

/* ---- Load image error */
#define CCCI_ERR_LOAD_IMG_NOMEM \
	(CCCI_ERR_LOAD_IMG_START_ID+0)
#define CCCI_ERR_LOAD_IMG_FILE_OPEN \
	(CCCI_ERR_LOAD_IMG_START_ID+1)
#define CCCI_ERR_LOAD_IMG_FILE_READ \
	(CCCI_ERR_LOAD_IMG_START_ID+2)
#define CCCI_ERR_LOAD_IMG_KERN_READ \
	(CCCI_ERR_LOAD_IMG_START_ID+3)
#define CCCI_ERR_LOAD_IMG_NO_ADDR \
	(CCCI_ERR_LOAD_IMG_START_ID+4)
#define CCCI_ERR_LOAD_IMG_NO_FIRST_BOOT \
	(CCCI_ERR_LOAD_IMG_START_ID+5)
#define CCCI_ERR_LOAD_IMG_LOAD_FIRM \
	(CCCI_ERR_LOAD_IMG_START_ID+6)
#define CCCI_ERR_LOAD_IMG_FIRM_NULL \
	(CCCI_ERR_LOAD_IMG_START_ID+7)
#define CCCI_ERR_LOAD_IMG_CHECK_HEAD \
	(CCCI_ERR_LOAD_IMG_START_ID+8)
#define CCCI_ERR_LOAD_IMG_SIGN_FAIL \
	(CCCI_ERR_LOAD_IMG_START_ID+9)
#define CCCI_ERR_LOAD_IMG_CIPHER_FAIL \
	(CCCI_ERR_LOAD_IMG_START_ID+10)
#define CCCI_ERR_LOAD_IMG_MD_CHECK \
	(CCCI_ERR_LOAD_IMG_START_ID+11)
#define CCCI_ERR_LOAD_IMG_DSP_CHECK \
	(CCCI_ERR_LOAD_IMG_START_ID+12)
#define CCCI_ERR_LOAD_IMG_ABNORAL_SIZE \
	(CCCI_ERR_LOAD_IMG_START_ID+13)
#define CCCI_ERR_LOAD_IMG_NOT_FOUND \
	(CCCI_ERR_LOAD_IMG_START_ID+13)

/* export to other kernel modules, */
/*better not let other module include ECCCI header directly (except IPC...) */
enum MD_STATE_FOR_USER {
	MD_STATE_INVALID = 0,
	MD_STATE_BOOTING = 1,
	MD_STATE_READY = 2,
	MD_STATE_EXCEPTION = 3
};

enum KERN_FUNC_ID {
	ID_GET_MD_WAKEUP_SRC,   /* for SPM */
	ID_GET_TXPOWER,		/* for thermal */
	ID_PAUSE_LTE,		/* for DVFS */
	ID_GET_MD_STATE,	/* for DVFS */
	ID_THROTTLING_CFG,	/* For MD SW throughput throttling */
	/* for dump MD debug info from SMEM when AP sleep */
	ID_DUMP_MD_SLEEP_MODE,
	/* for PMIC to notify MD buck over current, */
	/*called from kernel thread context */
	ID_PMIC_INTR,
	ID_FORCE_MD_ASSERT,	/* for EMI MPU */
	ID_MD_MPU_ASSERT,	/* for EMI MPU */
	ID_LWA_CONTROL_MSG,	/* for Wi-Fi driver */
	ID_UPDATE_TX_POWER,	/* for SWTP */
};

/* AP<->MD messages on control or system channel */
enum {
	/* Control channel, MD->AP */
	MD_INIT_START_BOOT = 0x0,
	MD_NORMAL_BOOT = 0x0,
	MD_NORMAL_BOOT_READY = 0x1, /* deprecated */
	MD_META_BOOT_READY = 0x2, /* deprecated */
	MD_RESET = 0x3, /* deprecated */
	MD_EX = 0x4,
	CCCI_DRV_VER_ERROR = 0x5,
	MD_EX_REC_OK = 0x6,
	MD_EX_RESUME = 0x7, /* deprecated */
	MD_EX_PASS = 0x8,
	MD_INIT_CHK_ID = 0x5555FFFF,
	MD_EX_CHK_ID = 0x45584350,
	MD_EX_REC_OK_CHK_ID = 0x45524543,

	/* System channel, AP->MD || AP<-->MD message start from 0x100 */
	MD_DORMANT_NOTIFY = 0x100, /* deprecated */
	MD_SLP_REQUEST = 0x101, /* deprecated */
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
	/* SWTP */
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

	SIM_LOCK_RANDOM_PATTERN = 0x118,
	CCISM_SHM_INIT = 0x119,
	CCISM_SHM_INIT_ACK = 0x11A,
	CCISM_SHM_INIT_DONE = 0x11B,
	PMIC_INTR_MODEM_BUCK_OC = 0x11C,
	MD_AP_MPU_ACK_MD = 0x11D,
	LWA_CONTROL_MSG = 0x11E,
	C2K_PPP_LINE_STATUS = 0x11F,	/*usb bypass for 93 and later*/
	MD_DISPLAY_DYNAMIC_MIPI = 0x120, /* MIPI for TC16 */

	/*c2k ctrl msg start from 0x200*/
	C2K_STATUS_IND_MSG = 0x201, /* for usb bypass */
	C2K_STATUS_QUERY_MSG = 0x202, /* for usb bypass */
	C2K_FLOW_CTRL_MSG = 0x205,
	C2K_HB_MSG = 0x207,
	C2K_CCISM_SHM_INIT = 0x209,
	C2K_CCISM_SHM_INIT_ACK = 0x20A,
	C2K_CCISM_SHM_INIT_DONE = 0x20B,

	/* System channel, MD->AP message start from 0x1000 */
	MD_WDT_MONITOR = 0x1000, /* deprecated */
	/* System channel, AP->MD message */
	MD_WAKEN_UP = 0x10000, /* deprecated */
};

enum {
	/*bit0-bit15:
	 *for modem capability related
	 *with ccci or ccci&ccmni driver
	 */
	MODEM_CAP_NAPI = (1<<0),
	MODEM_CAP_TXBUSY_STOP = (1<<1),
	MODEM_CAP_SGIO = (1<<2),
	/*bit16-bit31:
	 *for modem capability only
	 *related with ccmni driver
	 */
	MODEM_CAP_CCMNI_DISABLE = (1<<16),
	MODEM_CAP_DATA_ACK_DVD = (1<<17),
	MODEM_CAP_CCMNI_SEQNO = (1<<18),
	MODEM_CAP_CCMNI_IRAT = (1<<19),
	MODEM_CAP_WORLD_PHONE = (1<<20),
	/* it must depend on DATA ACK DEVIDE feature */
	MODEM_CAP_CCMNI_MQ = (1<<21),
	MODEM_CAP_DIRECT_TETHERING = (1<<22),
};

enum MD_STATE {
	INVALID = 0, /* no traffic */
	GATED, /* no traffic */
	BOOT_WAITING_FOR_HS1,
	BOOT_WAITING_FOR_HS2,
	READY,
	EXCEPTION,
	RESET, /* no traffic */
	WAITING_TO_STOP,
}; /* for CCCI and CCMNI, broadcast FSM */

enum HIF_STATE {
	RX_IRQ, /* broadcast by HIF, only for NAPI! */
	RX_FLUSH, /* broadcast by HIF only for GRO! */
	TX_IRQ, /* broadcast by HIF, only for network! */
	TX_FULL, /* broadcast by HIF, only for network! */
};

/* ============================================================== */
/* Image type and header defination part */
/* ============================================================== */
enum MD_IMG_TYPE {
	IMG_MD = 0,
	IMG_DSP,
	IMG_ARMV7,
	IMG_NUM,
};

enum PRODUCT_VER_TYPE {
	INVALID_VARSION = 0,
	DEBUG_VERSION,
	RELEASE_VERSION
};

#define IMG_NAME_LEN 32
#define IMG_POSTFIX_LEN 16
#define IMG_PATH_LEN 64

struct IMG_CHECK_INFO {
	char *product_ver;	/* debug/release/invalid */
	char *image_type;	/*2G/3G/invalid*/
	/* MT6573_S00(MT6573E1) or MT6573_S01(MT6573E2) */
	char *platform;
	char *build_time;	/* build time string */
	char *build_ver;	/* project version, ex:11A_MD.W11.28 */
	unsigned int mem_size; /*md rom+ram mem size*/
	unsigned int md_img_size; /*modem image actual size, exclude head size*/
	enum PRODUCT_VER_TYPE version;
	unsigned int header_verno;  /* header structure version number */
};

struct IMG_REGION_INFO {
	unsigned int  region_num;        /* total region number */
	struct _md_regin_info region_info[8];    /* max support 8 regions */
	/* max support 4 domain settings, each region has 4 control bits*/
	unsigned int  domain_attr[4];
};

struct ccci_image_info {
	enum MD_IMG_TYPE type;
	char file_name[IMG_PATH_LEN];
	phys_addr_t address; /* phy memory address to load this image */
	/* image size without signature, */
	/*cipher and check header, read form check header */
	unsigned int size;
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

enum SMEM_USER_ID {
	/* this should remain to be 0 for backward compatibility */
	SMEM_USER_RAW_DBM = 0,

	/* sequence in CCB users matters, must align with ccb_configs[]  */
	SMEM_USER_CCB_START,
	SMEM_USER_CCB_DHL = SMEM_USER_CCB_START,
	SMEM_USER_CCB_MD_MONITOR,
	SMEM_USER_CCB_META,
	SMEM_USER_CCB_END = SMEM_USER_CCB_META,

	/* squence of other users does not matter */
	SMEM_USER_RAW_CCB_CTRL,
	SMEM_USER_RAW_DHL,
	SMEM_USER_RAW_MDM,
	SMEM_USER_RAW_NETD,
	SMEM_USER_RAW_USB,
	SMEM_USER_RAW_AUDIO,
	SMEM_USER_RAW_DFD,
	SMEM_USER_RAW_LWA,
	SMEM_USER_RAW_MDCCCI_DBG,
	SMEM_USER_RAW_MDSS_DBG,
	SMEM_USER_RAW_RUNTIME_DATA,
	SMEM_USER_RAW_FORCE_ASSERT,
	SMEM_USER_CCISM_SCP,
	SMEM_USER_RAW_MD2MD,
	SMEM_USER_RAW_RESERVED,
	SMEM_USER_CCISM_MCU,
	SMEM_USER_CCISM_MCU_EXP,
	SMEM_USER_SMART_LOGGING,
	SMEM_USER_RAW_MD_CONSYS,
	SMEM_USER_RAW_PHY_CAP,
	SMEM_USER_MAX,
};

enum SYS_CB_ID {
	ID_GET_FDD_THERMAL_DATA = 0,
	ID_GET_TDD_THERMAL_DATA,
};

typedef int (*ccci_sys_cb_func_t)(int, int);
struct ccci_sys_cb_func_info {
	enum SYS_CB_ID		id;
	ccci_sys_cb_func_t	func;
};

#define MAX_KERN_API 64

/* ========================================================================== */
/* Export API */
/* ========================================================================== */
/* for getting modem info, Export by ccci util */
int ccci_get_fo_setting(char item[], unsigned int *val);
void ccci_md_mem_reserve(void);
unsigned int get_modem_is_enabled(int md_id);
unsigned int ccci_get_modem_nr(void);
int ccci_init_security(void);
int ccci_sysfs_add_modem(int md_id, void *kobj, void *ktype, get_status_func_t,
	boot_md_func_t);
int get_modem_support_cap(int md_id); /* Export by ccci util */
int set_modem_support_cap(int md_id, int new_val);
char *ccci_get_md_info_str(int md_id);
void get_md_postfix(int md_id, char k[], char buf[], char buf_ex[]);
void update_ccci_port_ver(unsigned int new_ver);
int ccci_load_firmware(int md_id, void *img_inf, char img_err_str[],
	char post_fix[], struct device *dev);
int get_md_resv_mem_info(int md_id, phys_addr_t *r_rw_base,
	unsigned int *r_rw_size, phys_addr_t *srw_base, unsigned int *srw_size);
int get_md_resv_ccb_info(int md_id, phys_addr_t *ccb_data_base,
	unsigned int *ccb_data_size);
int get_md1_md3_resv_smem_info(int md_id, phys_addr_t *rw_base,
	unsigned int *rw_size);
unsigned int get_md_resv_phy_cap_size(int md_id);
unsigned int get_md_smem_cachable_offset(int md_id);

unsigned long ccci_get_md_boot_count(int md_id); /* Export by ccci fsm */
int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf,
	unsigned int len); /* Export by ccci core */
int register_ccci_sys_call_back(int md_id, unsigned int id,
	ccci_sys_cb_func_t func); /* Export by ccci port */
void __iomem *get_smem_start_addr(int md_id, enum SMEM_USER_ID user_id,
	int *size_o); /* Export by ccci port */
int switch_sim_mode(int id, char *buf,
	unsigned int len); /* Export by SIM switch */
unsigned int get_sim_switch_type(void); /* Export by SIM switch */

#ifdef CONFIG_MTK_ECCCI_C2K
/* for c2k usb bypass */
int ccci_c2k_rawbulk_intercept(int ch_id, unsigned int interception);
int ccci_c2k_buffer_push(int ch_id, void *buf, int count);
int modem_dtr_set(int on, int low_latency);
int modem_dcd_state(void);
#endif
/* for modem get AP time */
void notify_time_update(void);
int wait_time_update_notify(void);
/* callback for system power off*/
void ccci_power_off(void);
/* LK load modem, Export by ccci util */
int modem_run_env_ready(int md_id);
int get_lk_load_md_info(char buf[], int size);
int get_md_type_from_lk(int md_id);
int get_raw_check_hdr(int md_id, char buf[], int size);
int ccci_get_md_check_hdr_inf(int md_id, void *img_inf, char post_fix[]);
int get_md_img_raw_size(int md_id);
void clear_meta_1st_boot_arg(int md_id);

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
void ccci_util_cmpt_mem_dump(int md_id, int buf_type, void *start_addr,
	int len);
int ccci_dump_write(int md_id, int buf_type, unsigned int flag,
	const char *fmt, ...);
int ccci_log_write(const char *fmt, ...);
int ccci_log_write_raw(unsigned int flags, const char *fmt, ...);
int ccci_event_log_cpy(char buf[], int size);
int ccci_event_log(const char *fmt, ...);
int ccmni_send_mbim_skb(int md_id, struct sk_buff *skb);
void ccmni_update_mbim_interface(int md_id, int id);

/* MPU setting */
struct _mpu_cfg {
	unsigned int start;
	unsigned int end;
	int region;
	unsigned int permission;
	int relate_region; /* Using same behavior and setting */
};
struct _mpu_cfg *get_mpu_region_cfg_info(int region_id);
int ccci_get_opt_val(char *opt_name);

/* RAT configure relate */
int ccci_get_rat_str_from_drv(int md_id, char rat_str[], int size);
void ccci_set_rat_str_to_drv(int md_id, char rat_str[]);
unsigned int get_wm_bitmap_for_ubin(void); /* Universal bin */
void update_rat_bit_map_to_drv(int md_id, unsigned int val);
int get_md_img_type(int md_id);
int get_legacy_md_type(int md_id);
int check_md_type(int data);

#endif
