#ifndef __MT_CCCI_COMMON_H__
#define __MT_CCCI_COMMON_H__
#include <asm/io.h>
#include <asm/setup.h>

/*
 * all code owned by CCCI should use modem index starts from ZERO
 */
typedef enum {
	MD_SYS1 = 0, // MD SYS name counts from 1, but internal index counts from 0
	MD_SYS2,
	MD_SYS3,
	MD_SYS4,
	MD_SYS5 = 4,
	MAX_MD_NUM
}MD_SYS;

// Meta parsing section
#define MD1_EN (1<<0)
#define MD2_EN (1<<1)
#define MD3_EN (1<<2)
#define MD5_EN (1<<4)

#define MD_2G_FLAG    (1<<0)
#define MD_FDD_FLAG   (1<<1)
#define MD_TDD_FLAG   (1<<2)
#define MD_LTE_FLAG   (1<<3)
#define MD_SGLTE_FLAG (1<<4)

#define MD_WG_FLAG  (MD_FDD_FLAG|MD_2G_FLAG)
#define MD_TG_FLAG  (MD_TDD_FLAG|MD_2G_FLAG)
#define MD_LWG_FLAG (MD_LTE_FLAG|MD_FDD_FLAG|MD_2G_FLAG)
#define MD_LTG_FLAG (MD_LTE_FLAG|MD_TDD_FLAG|MD_2G_FLAG)

#define CCCI_SMEM_DUMP_SIZE      4096// smem size we dump when EE
#define CCCI_SMEM_SIZE_EXCEPTION 0x200000//exception smem total size 
#define CCCI_SMEM_OFFSET_EXREC 2048// where the exception record begain in smem
#define CCCC_SMEM_CCIF_SRAM_SIZE 16
#define CCCI_SMEM_OFFSET_CCIF_SRAM (CCCI_SMEM_OFFSET_EXREC+1024-CCCC_SMEM_CCIF_SRAM_SIZE)
#define CCCI_SMEM_OFFSET_EPON 0xC64
#define CCCI_SMEM_OFFSET_SEQERR 0x34
#define CCCI_SMEM_OFFSET_CCCI_DEBUG 0 // where the MD CCCI debug info begain in smem
#define CCCI_SMEM_CCCI_DEBUG_SIZE 2048 // MD CCCI debug info size
#define CCCI_SMEM_OFFSET_MDSS_DEBUG 2048 // where the MD SS debug info begain in smem
#define CCCI_SMEM_MDSS_DEBUG_SIZE 2048 // MD SS debug info size

// MD type defination
typedef enum {
	md_type_invalid = 0,
	modem_2g = 1,
	modem_3g,
	modem_wg,
	modem_tg,
	modem_lwg,
	modem_ltg,
	modem_sglte,
	MAX_IMG_NUM = modem_sglte // this enum starts from 1
} MD_LOAD_TYPE;

// MD logger configure file
#define MD1_LOGGER_FILE_PATH "/data/mdlog/mdlog1_config"
#define MD2_LOGGER_FILE_PATH "/data/mdlog/mdlog2_config"

// Image string and header
// image name/path
#define MOEDM_IMAGE_NAME                "modem.img"
#define DSP_IMAGE_NAME                  "DSP_ROM"
#define CONFIG_MODEM_FIRMWARE_PATH      "/etc/firmware/"
#define CONFIG_MODEM_FIRMWARE_CIP_PATH  "/custom/etc/firmware/"
#define IMG_ERR_STR_LEN                 64

// image header constants
#define MD_HEADER_MAGIC_NO "CHECK_HEADER"

#define DEBUG_STR          "Debug"
#define RELEASE_STR        "Release"
#define INVALID_STR        "INVALID"

struct md_check_header {
	unsigned char check_header[12];  /* magic number is "CHECK_HEADER"*/
	unsigned int  header_verno;      /* header structure version number */
	unsigned int  product_ver;       /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int  image_type;        /* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned char platform[16];      /* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64];    /* build time string */
	unsigned char build_ver[64];     /* project version, ex:11A_MD.W11.28 */
	
	unsigned char bind_sys_id;       /* bind to md sys id, MD SYS1: 1, MD SYS2: 2 */
	unsigned char ext_attr;          /* no shrink: 0, shrink: 1*/
	unsigned char reserved[2];       /* for reserved */
	
	unsigned int  mem_size;          /* md ROM/RAM image size requested by md */
	unsigned int  md_img_size;       /* md image size, exclude head size*/
	unsigned int  reserved_info;     /* for reserved */
	unsigned int  size;              /* the size of this structure */
} __attribute__ ((packed));

struct md_check_header_v3 {
	unsigned char check_header[12];  /* magic number is "CHECK_HEADER"*/
	unsigned int  header_verno;      /* header structure version number */
	unsigned int  product_ver;       /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int  image_type;        /* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned char platform[16];      /* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64];    /* build time string */
	unsigned char build_ver[64];     /* project version, ex:11A_MD.W11.28 */
	
	unsigned char bind_sys_id;       /* bind to md sys id, MD SYS1: 1, MD SYS2: 2, MD SYS5: 5 */
	unsigned char ext_attr;          /* no shrink: 0, shrink: 1 */
	unsigned char reserved[2];       /* for reserved */

	unsigned int  mem_size;          /* md ROM/RAM image size requested by md */
	unsigned int  md_img_size;       /* md image size, exclude head size */
	unsigned int  rpc_sec_mem_addr;  /* RPC secure memory address */

	unsigned int  dsp_img_offset;
	unsigned int  dsp_img_size;
	unsigned char reserved2[88];
	
	unsigned int  size;              /* the size of this structure */
} __attribute__ ((packed));


//=================================================================================
// IOCTL defination
//=================================================================================
// CCCI == EEMCS
#define CCCI_IOC_MAGIC 'C'
#define CCCI_IOC_MD_RESET			_IO(CCCI_IOC_MAGIC, 0) // mdlogger // META // muxreport
#define CCCI_IOC_GET_MD_STATE			_IOR(CCCI_IOC_MAGIC, 1, unsigned int) // audio
#define CCCI_IOC_PCM_BASE_ADDR			_IOR(CCCI_IOC_MAGIC, 2, unsigned int) // audio
#define CCCI_IOC_PCM_LEN			_IOR(CCCI_IOC_MAGIC, 3, unsigned int) // audio
#define CCCI_IOC_FORCE_MD_ASSERT		_IO(CCCI_IOC_MAGIC, 4) // muxreport // mdlogger
#define CCCI_IOC_ALLOC_MD_LOG_MEM		_IO(CCCI_IOC_MAGIC, 5) // mdlogger
#define CCCI_IOC_DO_MD_RST			_IO(CCCI_IOC_MAGIC, 6) // md_init
#define CCCI_IOC_SEND_RUN_TIME_DATA		_IO(CCCI_IOC_MAGIC, 7) // md_init
#define CCCI_IOC_GET_MD_INFO			_IOR(CCCI_IOC_MAGIC, 8, unsigned int) // md_init
#define CCCI_IOC_GET_MD_EX_TYPE			_IOR(CCCI_IOC_MAGIC, 9, unsigned int) // mdlogger
#define CCCI_IOC_SEND_STOP_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 10) // muxreport
#define CCCI_IOC_SEND_START_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 11) // muxreport
#define CCCI_IOC_DO_STOP_MD			_IO(CCCI_IOC_MAGIC, 12) // md_init
#define CCCI_IOC_DO_START_MD			_IO(CCCI_IOC_MAGIC, 13) // md_init
#define CCCI_IOC_ENTER_DEEP_FLIGHT		_IO(CCCI_IOC_MAGIC, 14) // RILD // factory
#define CCCI_IOC_LEAVE_DEEP_FLIGHT		_IO(CCCI_IOC_MAGIC, 15) // RILD // factory
#define CCCI_IOC_POWER_ON_MD			_IO(CCCI_IOC_MAGIC, 16) // md_init
#define CCCI_IOC_POWER_OFF_MD			_IO(CCCI_IOC_MAGIC, 17) // md_init
#define CCCI_IOC_POWER_ON_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 18)
#define CCCI_IOC_POWER_OFF_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 19)
#define CCCI_IOC_SIM_SWITCH			_IOW(CCCI_IOC_MAGIC, 20, unsigned int) // RILD // factory
#define CCCI_IOC_SEND_BATTERY_INFO		_IO(CCCI_IOC_MAGIC, 21) // md_init 
#define CCCI_IOC_SIM_SWITCH_TYPE		_IOR(CCCI_IOC_MAGIC, 22, unsigned int) // RILD
#define CCCI_IOC_STORE_SIM_MODE			_IOW(CCCI_IOC_MAGIC, 23, unsigned int) // RILD
#define CCCI_IOC_GET_SIM_MODE			_IOR(CCCI_IOC_MAGIC, 24, unsigned int) // RILD
#define CCCI_IOC_RELOAD_MD_TYPE			_IO(CCCI_IOC_MAGIC, 25) // META // md_init // muxreport
#define CCCI_IOC_GET_SIM_TYPE			_IOR(CCCI_IOC_MAGIC, 26, unsigned int) // terservice
#define CCCI_IOC_ENABLE_GET_SIM_TYPE		_IOW(CCCI_IOC_MAGIC, 27, unsigned int) // terservice
#define CCCI_IOC_SEND_ICUSB_NOTIFY		_IOW(CCCI_IOC_MAGIC, 28, unsigned int) // icusbd
#define CCCI_IOC_SET_MD_IMG_EXIST		_IOW(CCCI_IOC_MAGIC, 29, unsigned int) // md_init
#define CCCI_IOC_GET_MD_IMG_EXIST		_IOR(CCCI_IOC_MAGIC, 30, unsigned int) // META
#define CCCI_IOC_GET_MD_TYPE			_IOR(CCCI_IOC_MAGIC, 31, unsigned int) // RILD
#define CCCI_IOC_STORE_MD_TYPE			_IOW(CCCI_IOC_MAGIC, 32, unsigned int) // RILD
#define CCCI_IOC_GET_MD_TYPE_SAVING		_IOR(CCCI_IOC_MAGIC, 33, unsigned int) // META
#define CCCI_IOC_GET_EXT_MD_POST_FIX		_IOR(CCCI_IOC_MAGIC, 34, unsigned int) // char[32] eemcs_fsd // mdlogger
#define CCCI_IOC_FORCE_FD			_IOW(CCCI_IOC_MAGIC, 35, unsigned int) // RILD(6577)
#define CCCI_IOC_AP_ENG_BUILD			_IOW(CCCI_IOC_MAGIC, 36, unsigned int) // md_init(6577)
#define CCCI_IOC_GET_MD_MEM_SIZE		_IOR(CCCI_IOC_MAGIC, 37, unsigned int) // md_init(6577)
#define CCCI_IOC_UPDATE_SIM_SLOT_CFG		_IOW(CCCI_IOC_MAGIC, 38, unsigned int) // RILD
#define CCCI_IOC_GET_CFG_SETTING		_IOW(CCCI_IOC_MAGIC, 39, unsigned int) // md_init

#define CCCI_IOC_SET_MD_SBP_CFG			_IOW(CCCI_IOC_MAGIC, 40, unsigned int) // md_init
#define CCCI_IOC_GET_MD_SBP_CFG			_IOW(CCCI_IOC_MAGIC, 41, unsigned int) // md_init
#define CCCI_IOC_GET_MD_PROTOCOL_TYPE	_IOR(CCCI_IOC_MAGIC, 42, char[16]) /*metal tool to get modem protocol type: AP_TST or DHL*/
#define CCCI_IOC_SEND_SIGNAL_TO_USER	_IOW(CCCI_IOC_MAGIC, 43, unsigned int) // md_init


#define CCCI_IOC_SET_HEADER				_IO(CCCI_IOC_MAGIC,  112)				// emcs_va
#define CCCI_IOC_CLR_HEADER				_IO(CCCI_IOC_MAGIC,  113)				// emcs_va
#define CCCI_IOC_DL_TRAFFIC_CONTROL		_IOW(CCCI_IOC_MAGIC, 119, unsigned int) // mdlogger

#define CCCI_IPC_MAGIC 'P' // only for IPC user
// CCCI == EEMCS
#define CCCI_IPC_RESET_RECV			_IO(CCCI_IPC_MAGIC,0)
#define CCCI_IPC_RESET_SEND			_IO(CCCI_IPC_MAGIC,1)
#define CCCI_IPC_WAIT_MD_READY			_IO(CCCI_IPC_MAGIC,2)
#define CCCI_IPC_KERN_WRITE_TEST		_IO(CCCI_IPC_MAGIC,3)

//=================================================================================
// CCCI Error number defination
//=================================================================================
// CCCI error number region
#define CCCI_ERR_MODULE_INIT_START_ID   (0)
#define CCCI_ERR_COMMON_REGION_START_ID	(100)
#define CCCI_ERR_CCIF_REGION_START_ID	(200)
#define CCCI_ERR_CCCI_REGION_START_ID	(300)
#define CCCI_ERR_LOAD_IMG_START_ID		(400)

// CCCI error number
#define CCCI_ERR_MODULE_INIT_OK                     (CCCI_ERR_MODULE_INIT_START_ID+0)
#define CCCI_ERR_INIT_DEV_NODE_FAIL                 (CCCI_ERR_MODULE_INIT_START_ID+1)
#define CCCI_ERR_INIT_PLATFORM_FAIL                 (CCCI_ERR_MODULE_INIT_START_ID+2)
#define CCCI_ERR_MK_DEV_NODE_FAIL                   (CCCI_ERR_MODULE_INIT_START_ID+3)
#define CCCI_ERR_INIT_LOGIC_LAYER_FAIL              (CCCI_ERR_MODULE_INIT_START_ID+4)
#define CCCI_ERR_INIT_MD_CTRL_FAIL                  (CCCI_ERR_MODULE_INIT_START_ID+5)
#define CCCI_ERR_INIT_CHAR_DEV_FAIL                 (CCCI_ERR_MODULE_INIT_START_ID+6)
#define CCCI_ERR_INIT_TTY_FAIL                      (CCCI_ERR_MODULE_INIT_START_ID+7)
#define CCCI_ERR_INIT_IPC_FAIL                      (CCCI_ERR_MODULE_INIT_START_ID+8)
#define CCCI_ERR_INIT_RPC_FAIL                      (CCCI_ERR_MODULE_INIT_START_ID+9)
#define CCCI_ERR_INIT_FS_FAIL                       (CCCI_ERR_MODULE_INIT_START_ID+10)
#define CCCI_ERR_INIT_CCMNI_FAIL                    (CCCI_ERR_MODULE_INIT_START_ID+11)
#define CCCI_ERR_INIT_VIR_CHAR_FAIL                 (CCCI_ERR_MODULE_INIT_START_ID+12)

// ---- Common
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

// ---- CCIF
#define CCCI_ERR_CCIF_NOT_READY						(CCCI_ERR_CCIF_REGION_START_ID+0)
#define CCCI_ERR_CCIF_CALL_BACK_HAS_REGISTERED		(CCCI_ERR_CCIF_REGION_START_ID+1)
#define CCCI_ERR_CCIF_GET_NULL_POINTER				(CCCI_ERR_CCIF_REGION_START_ID+2)
#define CCCI_ERR_CCIF_UN_SUPPORT					(CCCI_ERR_CCIF_REGION_START_ID+3)
#define CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL			(CCCI_ERR_CCIF_REGION_START_ID+4)
#define CCCI_ERR_CCIF_INVALID_RUNTIME_LEN			(CCCI_ERR_CCIF_REGION_START_ID+5)
#define CCCI_ERR_CCIF_INVALID_MD_SYS_ID				(CCCI_ERR_CCIF_REGION_START_ID+6)
#define CCCI_ERR_CCIF_GET_HW_INFO_FAIL				(CCCI_ERR_CCIF_REGION_START_ID+9)

// ---- CCCI
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

// ---- Load image error
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

//=================================================================================
// CCCI Channel ID and Message defination
//=================================================================================
typedef enum {
	CCCI_CONTROL_RX = 0,
	CCCI_CONTROL_TX = 1,
	CCCI_SYSTEM_RX = 2,
	CCCI_SYSTEM_TX = 3,
	CCCI_PCM_RX = 4,
	CCCI_PCM_TX = 5,
	CCCI_UART1_RX = 6, // META
	CCCI_UART1_RX_ACK = 7,
	CCCI_UART1_TX = 8,
	CCCI_UART1_TX_ACK = 9,
	CCCI_UART2_RX = 10, // MUX
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
	
	CCCI_MONITOR_CH,
	CCCI_DUMMY_CH,
	CCCI_MAX_CH_NUM, // RX channel ID should NOT be >= this!!

	CCCI_MONITOR_CH_ID = 0xf0000000, // for backward compatible
	CCCI_FORCE_ASSERT_CH = 20090215,
	CCCI_INVALID_CH_ID = 0xffffffff,
}CCCI_CH;

/* AP->md_init messages on monitor channel */
typedef enum {
	CCCI_MD_MSG_BOOT_READY		= 0xFAF50001,
	CCCI_MD_MSG_BOOT_UP		= 0xFAF50002,
	CCCI_MD_MSG_EXCEPTION		= 0xFAF50003,
	CCCI_MD_MSG_RESET		= 0xFAF50004,
	CCCI_MD_MSG_RESET_RETRY		= 0xFAF50005,
	CCCI_MD_MSG_READY_TO_RESET	= 0xFAF50006,
	CCCI_MD_MSG_BOOT_TIMEOUT	= 0xFAF50007,
	CCCI_MD_MSG_STOP_MD_REQUEST	= 0xFAF50008,
	CCCI_MD_MSG_START_MD_REQUEST	= 0xFAF50009,
	CCCI_MD_MSG_ENTER_FLIGHT_MODE	= 0xFAF5000A,
	CCCI_MD_MSG_LEAVE_FLIGHT_MODE	= 0xFAF5000B,
	CCCI_MD_MSG_POWER_ON_REQUEST	= 0xFAF5000C,
	CCCI_MD_MSG_POWER_OFF_REQUEST	= 0xFAF5000D,
	CCCI_MD_MSG_SEND_BATTERY_INFO   = 0xFAF5000E,
	CCCI_MD_MSG_NOTIFY		= 0xFAF5000F,
	CCCI_MD_MSG_STORE_NVRAM_MD_TYPE = 0xFAF50010,
	CCCI_MD_MSG_CFG_UPDATE	        = 0xFAF50011,
} CCCI_MD_MSG;


// export to other kernel modules, better not let other module include ECCCI header directly (except IPC...)
enum { 
	MD_STATE_INVALID = 0,
	MD_STATE_BOOTING = 1,
	MD_STATE_READY = 2,
	MD_STATE_EXCEPTION = 3
}; // align to MD_BOOT_STAGE

enum {
	ID_GET_MD_WAKEUP_SRC = 0,   // for SPM
	ID_CCCI_DORMANCY = 1,       // abandoned
	ID_LOCK_MD_SLEEP = 2,       // abandoned
	ID_ACK_MD_SLEEP = 3,        // abandoned
	ID_SSW_SWITCH_MODE = 4,     // abandoned
	ID_SET_MD_TX_LEVEL = 5,     // abandoned
	ID_GET_TXPOWER = 6,         // for thermal
	ID_IPO_H_RESTORE_CB = 7,    // abandoned
	ID_FORCE_MD_ASSERT = 8,     // abandoned
	ID_PAUSE_LTE = 9,		// for DVFS
	ID_STORE_SIM_SWITCH_MODE = 10,
	ID_GET_SIM_SWITCH_MODE = 11,
	ID_GET_MD_STATE = 12,		// for DVFS
	//used for throttling feature - start
	ID_THROTTLING_CFG = 13,		// For MD SW throughput throttling
	//used for throttling feature - end

	ID_UPDATE_TX_POWER = 100,   // for SWTP

};

enum {
	MODEM_CAP_NAPI = (1<<0),
	MODEM_CAP_TXBUSY_STOP = (1<<1),
};

/* AP<->MD messages on control or system channel */
enum { 
	// Control channel, MD->AP
	MD_INIT_START_BOOT = 0x0,
	MD_NORMAL_BOOT = 0x0,
	MD_NORMAL_BOOT_READY = 0x1, // not using
	MD_META_BOOT_READY = 0x2, // not using
	MD_RESET = 0x3, // not using
	MD_EX = 0x4, 
	CCCI_DRV_VER_ERROR = 0x5, 
	MD_EX_REC_OK = 0x6, 
	MD_EX_RESUME = 0x7, // not using
	MD_EX_PASS = 0x8,
	MD_INIT_CHK_ID = 0x5555FFFF,
	MD_EX_CHK_ID = 0x45584350,
	MD_EX_REC_OK_CHK_ID = 0x45524543,

	// System channel, AP->MD || AP<-->MD message start from 0x100
	MD_DORMANT_NOTIFY = 0x100,
	MD_SLP_REQUEST = 0x101,
	MD_TX_POWER = 0x102,
	MD_RF_TEMPERATURE = 0x103,
	MD_RF_TEMPERATURE_3G = 0x104,
	MD_GET_BATTERY_INFO = 0x105,
	MD_SIM_TYPE = 0x107,
	MD_ICUSB_NOTIFY = 0x108,
	//0x109 for md legacy use to crystal_thermal_change
	MD_LOW_BATTERY_LEVEL = 0x10A,
	// 0x10B-0x10C occupied by EEMCS
	MD_PAUSE_LTE = 0x10D,
	//used for throttling feature - start
	MD_THROTTLING = 0x112, // SW throughput throttling
	//used for throttling feature - end

	/* swtp */
	MD_SW_MD1_TX_POWER = 0x10E,
	MD_SW_MD2_TX_POWER = 0x10F,
	MD_SW_MD1_TX_POWER_REQ = 0x110,
	MD_SW_MD2_TX_POWER_REQ = 0x111,	

	// System channel, MD->AP message start from 0x1000
	MD_WDT_MONITOR = 0x1000,
	// System channel, AP->MD message
	MD_WAKEN_UP = 0x10000,
};

#define NORMAL_BOOT_ID 0
#define META_BOOT_ID 1

//=================================================================================
// Image type and header defination part
//=================================================================================
typedef enum {
	IMG_MD = 0,
	IMG_DSP,
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

struct IMG_CHECK_INFO{
	char *product_ver;	/* debug/release/invalid */
	char *image_type;	/*2G/3G/invalid*/
    char *platform;	    /* MT6573_S00(MT6573E1) or MT6573_S01(MT6573E2) */
	char *build_time;	/* build time string */
	char *build_ver;	/* project version, ex:11A_MD.W11.28 */
	unsigned int mem_size; /*md rom+ram mem size*/
	unsigned int md_img_size; /*modem image actual size, exclude head size*/
	PRODUCT_VER_TYPE version;
};

struct ccci_image_info
{
	MD_IMG_TYPE type;
	char file_name[IMG_PATH_LEN];
	phys_addr_t address; // phy memory address to load this image
	unsigned int size; // image size without signature, cipher and check header, read form check header
	unsigned int offset; // signature and cipher header
	unsigned int tail_length; // signature tail
	unsigned int dsp_offset;
	unsigned int dsp_size;
	char *ap_platform;
	struct IMG_CHECK_INFO img_info; // read from MD image header
	struct IMG_CHECK_INFO ap_info; // get from AP side configuration
};

struct ccci_dev_cfg {
	unsigned int index;
	unsigned int major;
	unsigned int minor_base;
	unsigned int capability;
};

typedef int (*get_status_func_t)(int,char*,int);
typedef int (*boot_md_func_t)(int);

// Rutime data common part
typedef enum
{
	FEATURE_NOT_EXIST = 0,
	FEATURE_NOT_SUPPORT,
	FEATURE_SUPPORT,
	FEATURE_PARTIALLY_SUPPORT,
} MISC_FEATURE_STATE; 

typedef enum
{
	MISC_DMA_ADDR = 0,
	MISC_32K_LESS,
	MISC_RAND_SEED,
	MISC_MD_COCLK_SETTING,
	MISC_MD_SBP_SETTING,
	MISC_MD_SEQ_CHECK,
} MISC_FEATURE_ID;

typedef enum {
    MODE_UNKNOWN = -1,      // -1
    MODE_IDLE,              // 0
    MODE_USB,               // 1
    MODE_SD,                // 2
    MODE_POLLING,           // 3
    MODE_WAITSD,            // 4
} LOGGING_MODE;

typedef enum {
    HIF_EX_INIT = 0, // interrupt
    HIF_EX_ACK, // AP->MD
    HIF_EX_INIT_DONE, // polling
    HIF_EX_CLEARQ_DONE, //interrupt
    HIF_EX_CLEARQ_ACK, // AP->MD
    HIF_EX_ALLQ_RESET, // polling
}HIF_EX_STAGE;

// runtime data format uses EEMCS's version, NOT the same with legacy CCCI
struct modem_runtime {
	u32 Prefix;             // "CCIF"
	u32 Platform_L;         // Hardware Platform String ex: "TK6516E0"
	u32 Platform_H;
	u32 DriverVersion;      // 0x00000923 since W09.23
	u32 BootChannel;        // Channel to ACK AP with boot ready
	u32 BootingStartID;     // MD is booting. NORMAL_BOOT_ID or META_BOOT_ID 
#if 1 // not using in EEMCS
	u32 BootAttributes;     // Attributes passing from AP to MD Booting
	u32 BootReadyID;        // MD response ID if boot successful and ready
	u32 FileShareMemBase;
	u32 FileShareMemSize;
	u32 ExceShareMemBase;
	u32 ExceShareMemSize;
	u32 CCIFShareMemBase;
	u32 CCIFShareMemSize;
	u32 TotalShareMemBase;
	u32 TotalShareMemSize;
	u32 CheckSum;
#endif
	u32 Postfix;            //"CCIF" 
#if 1 // misc region
	u32 misc_prefix;	// "MISC"
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
	u32 misc_postfix;	// "MISC"
#endif
} __attribute__ ((packed));

typedef enum {
	ID_GET_FDD_THERMAL_DATA = 0,
	ID_GET_TDD_THERMAL_DATA,
}SYS_CB_ID;

typedef int (*ccci_sys_cb_func_t)(int, int);
typedef struct{
	SYS_CB_ID		id;
	ccci_sys_cb_func_t	func;
}ccci_sys_cb_func_info_t;

#define MAX_KERN_API 20

//==============================================================================================
// Export API
//==============================================================================================
int ccci_get_fo_setting(char item[], unsigned int *val); // Export by ccci util
void ccci_md_mem_reserve(void); // Export by ccci util
unsigned int get_modem_is_enabled(int md_id); // Export by ccci util
unsigned int ccci_get_modem_nr(void); // Export by ccci util
int ccci_init_security(void); // Export by ccci util
int ccci_sysfs_add_modem(int md_id, void *kobj, void *ktype, get_status_func_t, boot_md_func_t); // Export by ccci util
int get_modem_support_cap(int md_id); // Export by ccci util
int set_modem_support_cap(int md_id, int new_val); // Export by ccci util
char* ccci_get_md_info_str(int md_id); // Export by ccci util
int ccci_load_firmware(int md_id, void* img_inf, char img_err_str[], char post_fix[]); // Export by ccci util
int get_md_resv_mem_info(int md_id, phys_addr_t *r_rw_base, unsigned int *r_rw_size, phys_addr_t *srw_base, unsigned int *srw_size); // Export by ccci util
//used for throttling feature - start
unsigned long ccci_get_md_boot_count(int md_id);
//used for throttling feature - end

int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf, unsigned int len);
int register_ccci_sys_call_back(int md_id, unsigned int id, ccci_sys_cb_func_t func);
int switch_sim_mode(int id, char *buf, unsigned int len);
unsigned int get_sim_switch_type(void);

#endif
