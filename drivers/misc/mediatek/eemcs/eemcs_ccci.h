#ifndef __EEMCS_CCCI_H__
#define __EEMCS_CCCI_H__
#include <linux/version.h>
#include "eemcs_kal.h"
#include "lte_df_main.h"

#define BLK_16K			(16384)
#define BLK_8K		    (8192)
#define BLK_4K          (4096)
#define BLK_2K          (2048)
#define BLK_1K          (1024)

#ifndef CCCI_MTU_3456B
#define MAX_TX_BYTE     (0xFFF - 127)
#else
#define MAX_TX_BYTE     (3584 - 128) //3456B, skb allocate max size=3.5KB, CCCI header+reserved mem=128B
#endif

#define MD_IMG_MAX_CNT  (7)

typedef KAL_INT32 (*EEMCS_CCCI_CALLBACK)(struct sk_buff *skb, KAL_UINT32 data);
typedef KAL_INT32 (*EEMCS_CCCI_SWINT_CALLBACK)(KAL_UINT32 swint_status);
typedef KAL_INT32 (*EEMCS_CCCI_WDT_CALLBACK)(void);

enum
{
	EXPORT_CCCI_H =(1<<0),
	TX_PRVLG1 =(1<<1),		// tx channel can send data even if modem not boot ready
	TX_PRVLG2 =(1<<2),		// This logic channel can send data even if modem exception
};

typedef struct{
    KAL_UINT8 rx;
    KAL_UINT8 tx;
    EEMCS_CCCI_CALLBACK rx_cb;
    EEMCS_CCCI_CALLBACK tx_cb; /* reserved */
}ccci_ch_set;

typedef struct
{
    ccci_ch_set ch;
    KAL_UINT32  hif_type;
    KAL_UINT32 	txq_id;
    KAL_UINT32 	rxq_id;
    KAL_UINT32  rx_flow_ctrl_limit; //>0, enable flow ctl and disable rxq read when rx_cnt>limit; =0, disable flow ctrl
    KAL_UINT32  rx_flow_ctrl_thresh;//enable rxq read when rx_cnt<thresh

    KAL_UINT32  tx_mem_size;
    KAL_UINT32  tx_mem_cnt;
    KAL_UINT32  rx_mem_size;
    KAL_UINT32  rx_mem_cnt;
    
    KAL_UINT32  export_type; //EXPORT_CCCI_H
    KAL_UINT32  pri;
	KAL_UINT32  flag;
    atomic_t    reserve_space; //use by blocking I/O
}ccci_port_cfg;

typedef struct{
    wait_queue_head_t tx_waitq;
    atomic_t    reserve_space;
}ccci_tx_waitq_t;

enum HIF_TYPE{
    HIF_CCIF,
    HIF_SDIO,    
    HIF_CLDMA,    
};

enum PORT_PRI{
    PRI_RT,
    PRI_NR,
};

enum EXPORT_TYPE{
    EX_T_USER,
    EX_T_KERN,    
    EX_T_BOOT,    
};

typedef enum{
    TX_Q_MIN,
    TX_Q_0 = TX_Q_MIN,
    TX_Q_1,
    TX_Q_2,
    TX_Q_3,
    TX_Q_4,
    TX_Q_MAX,
    RX_Q_MIN = TX_Q_MAX,
    RX_Q_0 = RX_Q_MIN,
    RX_Q_1,
    RX_Q_2,
    RX_Q_3,
    RX_Q_MAX,
    RX_Q_BOOT = RX_Q_MAX,
    TR_Q_NUM,
    TR_Q_INVALID,
}SDIO_QUEUE_IDX;

#define SDIO_TX_Q_NUM (TX_Q_MAX - TX_Q_MIN) //TX_Q_NUM
#define SDIO_RX_Q_NUM (RX_Q_MAX - RX_Q_MIN) //RX_Q_NUM
#define SDIO_TXQ(x)   (x - TX_Q_0)          //CCCI_TXQ_TO_DF
#define SDIO_RXQ(x)   (x - RX_Q_0)          //CCCI_RXQ_TO_DF

typedef enum { /* sync with MD */
    CH_CTRL_RX = 0,
    CH_CTRL_TX = 1,
    CH_SYS_RX = 2,
    CH_SYS_TX = 3,
    CH_AUD_RX = 4,
    CH_AUD_TX = 5,
    CH_META_RX = 6,
    CH_META_RACK = 7,
    CH_META_TX = 8,
    CH_META_TACK = 9,
    CH_MUX_RX = 10,
    CH_MUX_RACK = 11,
    CH_MUX_TX = 12,
    CH_MUX_TACK = 13,
    CH_FS_RX = 14,
    CH_FS_TX = 15,
    CH_PMIC_RX = 16,
    CH_PMIC_TX = 17,
    CH_UEM_RX = 18,
    CH_UEM_TX = 19,
    CH_NET1_RX = 20,
    CH_NET1_RX_ACK = 21,
    CH_NET1_TX = 22,
    CH_NET1_TX_ACK = 23,
    CH_NET2_RX = 24,
    CH_NET2_RX_ACK = 25,
    CH_NET2_TX = 26,
    CH_NET2_TX_ACK = 27,
    CH_NET3_RX = 28,
    CH_NET3_RX_ACK = 29,
    CH_NET3_TX = 30,
    CH_NET3_TX_ACK = 31,
    CH_RPC_RX = 32,
    CH_RPC_TX = 33,
    CH_IPC_RX = 34,
    CH_IPC_RX_ACK = 35,
    CH_IPC_TX = 36,
    CH_IPC_TX_ACK = 37,
    CH_AGPS_RX = 38,
    CH_AGPS_RX_ACK = 39,
    CH_AGPS_TX = 40,
    CH_AGPS_TX_ACK = 41,
    CH_MLOG_RX = 42,
    CH_MLOG_TX = 43,
    /* ch44~49 reserved for ARM7 */
    CH_IT_RX = 50,
    CH_IT_TX = 51,
    CH_IMSV_UL = 52,
    CH_IMSV_DL = 53,
    CH_IMSC_UL = 54,
    CH_IMSC_DL = 55,
    CH_IMSA_UL = 56,
    CH_IMSA_DL = 57,
    CH_IMSDC_UL = 58,
    CH_IMSDC_DL = 59,
    CH_NET1_DL_ACK = 64, /* ch for CCMNI0 ACK packet of DL data packet*/
    CH_NET2_DL_ACK = 65, /* ch for CCMNI1 ACK packet of DL data packet */
    CH_NET3_DL_ACK = 66, /* ch for CCMNI2 ACK packet of DL data packet */
    CH_NUM_MAX,
    CH_DUMMY = 0xFF,   /* ch which drops Tx pkts */
    CCCI_FORCE_RESET_MODEM_CHANNEL  = 20090215,
}CCCI_CHANNEL_T;

enum CCCI_PORT{
    /* CCCI Character Devices, start from 0 */
    START_OF_CCCI_CDEV = 0x00,
    START_OF_BOOT_PORT = START_OF_CCCI_CDEV,
    CCCI_PORT_CTRL = START_OF_BOOT_PORT,/*PORT=0*/
    END_OF_BOOT_PORT,
    START_OF_NORMAL_PORT = END_OF_BOOT_PORT,
    CCCI_PORT_SYS = START_OF_NORMAL_PORT,/*PORT=1*/
    CCCI_PORT_AUD,        /*PORT=2*/
    CCCI_PORT_META,       /*PORT=3*/
    CCCI_PORT_MUX,     	  /*PORT=4*/
    CCCI_PORT_FS,     	  /*PORT=5*/
    CCCI_PORT_PMIC,       /*PORT=6*/
    CCCI_PORT_UEM,     	  /*PORT=7*/
    CCCI_PORT_RPC,     	  /*PORT=8*/
    CCCI_PORT_IPC,     	  /*PORT=9*/
    CCCI_PORT_IPC_UART,   /*PORT=10*/
    CCCI_PORT_MD_LOG,     /*PORT=11*/
    CCCI_PORT_IMS_VIDEO,  /*PORT=12*/
    CCCI_PORT_IMS_CTRL,   /*PORT=13*/   
    CCCI_PORT_IMS_AUDIO,  /*PORT=14*/
    CCCI_PORT_IMS_DCTRL,  /*PORT=15*/
    CCCI_PORT_MUX_REPORT, /*PORT=16, ioctl only no CCCI ch needed */
    CCCI_PORT_IOCTL,      /*PORT=17, ioctl only no CCCI ch needed */
    CCCI_PORT_RILD,       /*PORT=18, ioctl only no CCCI ch needed */
    CCCI_PORT_IT,         /*PORT=19, ioctl only no CCCI ch needed */
    END_OF_NORMAL_PORT,
    END_OF_CCCI_CDEV = END_OF_NORMAL_PORT,
    
    /* CCCI Network Interface */
    START_OF_CCMNI = END_OF_CCCI_CDEV,
    CCCI_PORT_NET1 = START_OF_CCMNI, /*PORT=20*/
    CCCI_PORT_NET2,  /*PORT=21*/
    CCCI_PORT_NET3,  /*PORT=22*/
    END_OF_CCMNI,

    CCCI_PORT_NUM_MAX = END_OF_CCMNI,										
};


typedef struct
{   
    union{
        KAL_UINT32 data[2];
        struct {
            KAL_UINT32 data0;
            KAL_UINT32 data1;
        };
        struct {
            KAL_UINT32 magic;
            KAL_UINT32 id;
        };
    };
    KAL_UINT32 channel;
    KAL_UINT32 reserved;
} CCCI_BUFF_T;

typedef struct
{
    unsigned int tx_cnt:12;
    unsigned int reserd:17;
    unsigned int d_type:3;
}SDIO_H;

#define CCCI_MAGIC_NUM     0xFFFFFFFF

/* CCCI/EMCS ioctl messages */
// CCCI == EEMCS
#define CCCI_IOC_MAGIC 'C'
#define CCCI_IOC_MD_RESET				_IO(CCCI_IOC_MAGIC, 0) // mdlogger // META // muxreport
#define CCCI_IOC_GET_MD_STATE			_IOR(CCCI_IOC_MAGIC, 1, unsigned int) // audio
#define CCCI_IOC_PCM_BASE_ADDR			_IOR(CCCI_IOC_MAGIC, 2, unsigned int) // audio
#define CCCI_IOC_PCM_LEN				_IOR(CCCI_IOC_MAGIC, 3, unsigned int) // audio
#define CCCI_IOC_FORCE_MD_ASSERT		_IO(CCCI_IOC_MAGIC, 4) // muxreport // mdlogger
#define CCCI_IOC_ALLOC_MD_LOG_MEM		_IO(CCCI_IOC_MAGIC, 5) // mdlogger
#define CCCI_IOC_DO_MD_RST				_IO(CCCI_IOC_MAGIC, 6) // md_init
#define CCCI_IOC_SEND_RUN_TIME_DATA		_IO(CCCI_IOC_MAGIC, 7) // md_init
#define CCCI_IOC_GET_MD_INFO			_IOR(CCCI_IOC_MAGIC, 8, unsigned int) // md_init
#define CCCI_IOC_GET_MD_EX_TYPE			_IOR(CCCI_IOC_MAGIC, 9, unsigned int) // mdlogger
#define CCCI_IOC_SEND_STOP_MD_REQUEST	_IO(CCCI_IOC_MAGIC, 10) // muxreport
#define CCCI_IOC_SEND_START_MD_REQUEST	_IO(CCCI_IOC_MAGIC, 11) // muxreport
#define CCCI_IOC_DO_STOP_MD				_IO(CCCI_IOC_MAGIC, 12) // md_init
#define CCCI_IOC_DO_START_MD			_IO(CCCI_IOC_MAGIC, 13) // md_init
#define CCCI_IOC_ENTER_DEEP_FLIGHT		_IO(CCCI_IOC_MAGIC, 14) // RILD // factory
#define CCCI_IOC_LEAVE_DEEP_FLIGHT		_IO(CCCI_IOC_MAGIC, 15) // RILD // factory
#define CCCI_IOC_POWER_ON_MD			_IO(CCCI_IOC_MAGIC, 16) // md_init
#define CCCI_IOC_POWER_OFF_MD			_IO(CCCI_IOC_MAGIC, 17) // md_init
#define CCCI_IOC_POWER_ON_MD_REQUEST	_IO(CCCI_IOC_MAGIC, 18)
#define CCCI_IOC_POWER_OFF_MD_REQUEST	_IO(CCCI_IOC_MAGIC, 19)
#define CCCI_IOC_SIM_SWITCH				_IOW(CCCI_IOC_MAGIC, 20, unsigned int) // RILD // factory
#define CCCI_IOC_SEND_BATTERY_INFO		_IO(CCCI_IOC_MAGIC, 21) // md_init 
#define CCCI_IOC_SIM_SWITCH_TYPE		_IOR(CCCI_IOC_MAGIC, 22, unsigned int) // RILD
#define CCCI_IOC_STORE_SIM_MODE			_IOW(CCCI_IOC_MAGIC, 23, unsigned int) // RILD
#define CCCI_IOC_GET_SIM_MODE			_IOR(CCCI_IOC_MAGIC, 24, unsigned int) // RILD
#define CCCI_IOC_RELOAD_MD_TYPE			_IO(CCCI_IOC_MAGIC, 25) // META // md_init // muxreport
#define CCCI_IOC_GET_SIM_TYPE			_IOR(CCCI_IOC_MAGIC, 26, unsigned int) // terservice
#define CCCI_IOC_ENABLE_GET_SIM_TYPE	_IOW(CCCI_IOC_MAGIC, 27, unsigned int) // terservice
#define CCCI_IOC_SEND_ICUSB_NOTIFY		_IOW(CCCI_IOC_MAGIC, 28, unsigned int) // icusbd
#define CCCI_IOC_SET_MD_IMG_EXIST		_IOW(CCCI_IOC_MAGIC, 29, unsigned int) // md_init
#define CCCI_IOC_GET_MD_IMG_EXIST		_IOR(CCCI_IOC_MAGIC, 30, unsigned int)
#define CCCI_IOC_GET_MD_TYPE			_IOR(CCCI_IOC_MAGIC, 31, unsigned int) // RILD
#define CCCI_IOC_STORE_MD_TYPE			_IOW(CCCI_IOC_MAGIC, 32, unsigned int) // RILD
#define CCCI_IOC_GET_MD_TYPE_SAVING		_IOR(CCCI_IOC_MAGIC, 33, unsigned int) // META
#define CCCI_IOC_GET_EXT_MD_POST_FIX	_IOR(CCCI_IOC_MAGIC, 34, unsigned int) // char[32] emcs_fsd // mdlogger
#define CCCI_IOC_FORCE_FD				_IOW(CCCI_IOC_MAGIC, 35, unsigned int) // RILD(6577)
#define CCCI_IOC_AP_ENG_BUILD			_IOW(CCCI_IOC_MAGIC, 36, unsigned int) // md_init(6577)
#define CCCI_IOC_GET_MD_MEM_SIZE		_IOR(CCCI_IOC_MAGIC, 37, unsigned int) // md_init(6577)
#define CCCI_IOC_GET_CFG_SETTING		_IOW(CCCI_IOC_MAGIC, 39, unsigned int) // md_init

#define CCCI_IOC_GET_MD_PROTOCOL_TYPE	_IOR(CCCI_IOC_MAGIC, 42, char[16]) /*metal tool to get modem protocol type: AP_TST or DHL*/

// EEMCS only
#define CCCI_IOC_BOOT_MD				_IO(CCCI_IOC_MAGIC,  100)				/*mdinit*/
#define CCCI_IOC_GATE_MD				_IO(CCCI_IOC_MAGIC,  101)				/*nouser*/
#define CCCI_IOC_ASSERT_MD				_IO(CCCI_IOC_MAGIC,  102)				/*nouser*/
#define CCCI_IOC_CHECK_STATE			_IOR(CCCI_IOC_MAGIC, 103, unsigned int)	/*nouser*/
#define CCCI_IOC_SET_STATE				_IOW(CCCI_IOC_MAGIC, 104, unsigned int)	/*nouser*/
#define CCCI_IOC_GET_MD_BOOT_INFO		_IOR(CCCI_IOC_MAGIC, 105, unsigned int)	/*mdinit*/
#define CCCI_IOC_START_BOOT				_IO(CCCI_IOC_MAGIC,  106)				/*mdinit*/
#define CCCI_IOC_BOOT_DONE				_IO(CCCI_IOC_MAGIC,  107)				/*mdinit*/
#define CCCI_IOC_REBOOT					_IO(CCCI_IOC_MAGIC,  108)				/*mdinit*/
#define CCCI_IOC_MD_EXCEPTION			_IO(CCCI_IOC_MAGIC,  109)				/*nouser*/
#define CCCI_IOC_MD_EX_REC_OK			_IO(CCCI_IOC_MAGIC,  110)				/*nouser*/
#define CCCI_IOC_GET_RUNTIME_DATA		_IOR(CCCI_IOC_MAGIC, 111, char[1024])	/*mdinit*/
#define CCCI_IOC_SET_HEADER				_IO(CCCI_IOC_MAGIC,  112)				/*UT    */
#define CCCI_IOC_CLR_HEADER				_IO(CCCI_IOC_MAGIC,  113)				/*UT    */
#define CCCI_IOC_SET_EXCEPTION_DATA		_IOW(CCCI_IOC_MAGIC, 114, char[1024])	/*nouser*/
#define CCCI_IOC_GET_EXCEPTION_LENGTH	_IOR(CCCI_IOC_MAGIC, 115, unsigned int)	/*nouser*/
#define CCCI_IOC_SET_BOOT_STATE			_IOW(CCCI_IOC_MAGIC, 116, unsigned int)	/*boot_IT*/
#define CCCI_IOC_GET_BOOT_STATE			_IOR(CCCI_IOC_MAGIC, 117, unsigned int)	/*boot_IT*/
#define CCCI_IOC_WAIT_RDY_RST			_IO(CCCI_IOC_MAGIC,  118)				/*mdinit*/ /* For MD reset flow must wait for mux/fsd/mdlogger close port */
#define CCCI_IOC_DL_TRAFFIC_CONTROL		_IOW(CCCI_IOC_MAGIC, 119, unsigned int)	/* For MD LOGER to turn on/off downlink traffic */
#define CCCI_IOC_FLOW_CTRL_SETTING      _IOW(CCCI_IOC_MAGIC, 120, unsigned int)	/*flow control setting*/
#define CCCI_IOC_BOOT_UP_TIMEOUT        _IOW(CCCI_IOC_MAGIC, 121, unsigned int[2])	/*notify kernel of md boot up timeout*/
#define CCCI_IOC_SET_BOOT_TO_VAL	_IOW(CCCI_IOC_MAGIC, 122, unsigned int)  /* Set boot time out value */

/*******************************************************************************
*                            CCCI_ERROR_CODE
********************************************************************************/

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
#define CCCI_ERR_INIT_SYSMSG_FAIL                   (CCCI_ERR_MODULE_INIT_START_ID+13)

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



/*******************************************************************************
*                            A P I s
********************************************************************************/
KAL_UINT32 ccci_get_port_type(KAL_UINT32 ccci_port_index);
ccci_port_cfg* ccci_get_port_info(KAL_UINT32 ccci_port_index);
KAL_UINT32 ccci_get_port_cflag(KAL_UINT32 ccci_port_index);
void ccci_set_port_type(KAL_UINT32 ccci_port_index, KAL_UINT32 new_flag);

void eemcs_ccci_turn_off_dlq_by_port(KAL_UINT32 ccci_port_index);
void eemcs_ccci_turn_on_dlq_by_port(KAL_UINT32 ccci_port_index);

void eemcs_ccci_release_rx_skb(KAL_UINT32 port_idx, KAL_UINT32 cnt, struct sk_buff *skb);
void eemcs_ccci_reset(void);

KAL_UINT32 eemcs_ccci_register_callback(CCCI_CHANNEL_T chn, EEMCS_CCCI_CALLBACK func_ptr , KAL_UINT32 private_data);
KAL_UINT32 eemcs_ccci_register_swint_callback(EEMCS_CCCI_SWINT_CALLBACK func_ptr);
KAL_UINT32 eemcs_ccci_register_WDT_callback(EEMCS_CCCI_WDT_CALLBACK func_ptr);
KAL_UINT32 eemcs_ccci_unregister_callback(CCCI_CHANNEL_T chn);
KAL_UINT32 eemcs_ccci_unregister_swint_callback(KAL_UINT32 id);
KAL_UINT32 eemcs_ccci_unregister_WDT_callback(void);
KAL_UINT32 eemcs_ccci_UL_write_room_alloc(CCCI_CHANNEL_T chn);
KAL_UINT32 eemcs_ccci_UL_write_room_release(CCCI_CHANNEL_T chn);
KAL_UINT32 eemcs_ccci_UL_write_wait(CCCI_CHANNEL_T chn);
KAL_INT32 eemcs_ccci_UL_write_skb_to_swq(CCCI_CHANNEL_T chn, struct sk_buff *skb);
KAL_UINT32 eemcs_ccci_boot_UL_write_room_check(void);
KAL_INT32 eemcs_ccci_boot_UL_write_skb_to_swq(struct sk_buff *skb);
//inline struct sk_buff* eemcs_ccci_DL_read_skb_from_swq(CCCI_CHANNEL_T chn);
KAL_UINT32 ccci_ch_to_port(KAL_UINT32 ccci_ch_num);
KAL_INT32 eemcs_ccci_mod_init(void);
void eemcs_ccci_exit(void);
int eemcs_cdev_msg(int port_id, unsigned int message, unsigned int reserved);
#ifdef _EEMCS_CCCI_LB_UT
/* UL APIs */
int ccci_ut_UL_write_skb_to_swq(MTLTE_DF_TX_QUEUE_TYPE qno , struct sk_buff *skb);
int ccci_ut_UL_swq_space(MTLTE_DF_TX_QUEUE_TYPE qno);
int ccci_ut_register_swint_callback(MTLTE_DF_TO_DEV_CALLBACK func_ptr);
int ccci_ut_unregister_swint_callback(void);
int ccci_ut_register_WDT_callback(MTLTE_DF_TO_DEV_CALLBACK func_ptr);
int ccci_ut_unregister_WDT_callback(void);
int ccci_ut_register_tx_callback(MTLTE_DF_TX_QUEUE_TYPE qno, MTLTE_DF_TO_DEV_CALLBACK func_ptr , unsigned int private_data);
/* DL APIs */
struct sk_buff * ccci_ut_DL_read_skb_from_swq(MTLTE_DF_RX_QUEUE_TYPE qno);
int ccci_ut_DL_pkt_handle_complete(MTLTE_DF_RX_QUEUE_TYPE qno);
int ccci_ut_register_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno, MTLTE_DF_TO_DEV_CALLBACK func_ptr , unsigned int private_data);
void ccci_ut_unregister_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno);

void ccci_ut_expt_q_num_init(KAL_UINT32 nonstop_q, KAL_UINT32 except_q);
int ccci_ut_register_expt_callback(EEMCS_CCCI_EX_IND func_ptr);

void ccci_ut_init_probe(void);
void ccci_ut_exit(void);

void ccci_ut_turnoff_DL_port(MTLTE_DF_RX_QUEUE_TYPE qno);
void ccci_ut_turnon_DL_port(MTLTE_DF_RX_QUEUE_TYPE qno);

/* UT mode */
#define hif_ul_write_swq              ccci_ut_UL_write_skb_to_swq
#define hif_ul_swq_space              ccci_ut_UL_swq_space
#define hif_reg_swint_cb              ccci_ut_register_swint_callback
#define hif_unreg_swint_cb            ccci_ut_unregister_swint_callback
#define hif_reg_WDT_cb                ccci_ut_register_WDT_callback
#define hif_unreg_WDT_cb              ccci_ut_unregister_WDT_callback
#define hif_dl_read_swq               ccci_ut_DL_read_skb_from_swq
#define hif_dl_pkt_handle_complete    ccci_ut_DL_pkt_handle_complete
#define hif_reg_rx_cb                 ccci_ut_register_rx_callback
#define hif_unreg_rx_cb               ccci_ut_unregister_rx_callback
#define hif_reg_tx_cb                 ccci_ut_register_tx_callback
#define hif_unreg_tx_cb               ccci_ut_unregister_tx_callback
#define hif_clean_tq_cnt              ccci_ut_clean_txq_count
#define hif_except_init               ccci_ut_expt_q_num_init
#define hif_reg_expt_cb               ccci_ut_register_expt_callback
#define hif_turn_off_dl_q             ccci_ut_turnoff_DL_port
#define hif_turn_on_dl_q              ccci_ut_turnon_DL_port
#else
void eemcs_ccci_exit(void);

/* normal mode */
#define hif_ul_write_swq              mtlte_df_UL_write_skb_to_swq
#define hif_ul_swq_space              mtlte_df_UL_swq_space
#define hif_reg_swint_cb              mtlte_df_register_swint_callback
#define hif_unreg_swint_cb            mtlte_df_unregister_swint_callback
#define hif_reg_WDT_cb                mtlte_df_register_WDT_callback
#define hif_unreg_WDT_cb              mtlte_df_unregister_WDT_callback
#define hif_dl_read_swq               mtlte_df_DL_read_skb_from_swq
#define hif_dl_pkt_handle_complete    mtlte_df_DL_pkt_handle_complete
#define hif_reg_rx_cb                 mtlte_df_register_rx_callback
#define hif_unreg_rx_cb               mtlte_df_unregister_rx_callback
#define hif_reg_tx_cb                 mtlte_df_register_tx_callback
#define hif_unreg_tx_cb               mtlte_df_unregister_tx_callback
#define hif_clean_tq_cnt              mtlte_hif_clean_txq_count
#define hif_except_init               mtlte_expt_q_num_init
#define hif_reg_expt_cb               mtlte_expt_register_callback
#define hif_turn_off_dl_q             mtlte_manual_turnoff_DL_port
#define hif_turn_on_dl_q              mtlte_manual_turnon_DL_port
#endif

#endif // __EEMCS_CCCI_H__
