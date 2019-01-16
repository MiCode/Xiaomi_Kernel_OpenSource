#ifndef __PORT_KERNEL_H__
#define __PORT_KERNEL_H__

#include <ccci_core.h>

#define CCCI_AED_DUMP_EX_MEM		(1<<0)
#define CCCI_AED_DUMP_MD_IMG_MEM	(1<<1)
#define CCCI_AED_DUMP_CCIF_REG		(1<<2)
#define CCCI_AED_DUMP_EX_PKT		(1<<3)

#define EE_BUF_LEN		(256)
#define AED_STR_LEN		(512)

#define CCCI_EXREC_OFFSET_OFFENDER 288

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
	NUM_EXCEPTION,
	
	MD_EX_TYPE_EMI_CHECK = 99,
};

enum {
	MD_EE_FLOW_START = 0,
	MD_EE_DUMP_ON_GOING,
	MD_STATE_UPDATE,
	MD_EE_MSG_GET,
	MD_EE_TIME_OUT_SET,
	MD_EE_OK_MSG_GET,
	MD_EE_FOUND_BY_ISR, // not using
	MD_EE_FOUND_BY_TX, // not using
	MD_EE_PENDING_TOO_LONG,
	MD_EE_SWINT_GET,
	MD_EE_WDT_GET,
	
	MD_EE_INFO_OFFSET = 20, // not using
	MD_EE_EXCP_OCCUR = 20, // not using
	MD_EE_AP_MASK_I_BIT_TOO_LONG = 21, // not using
	MD_EE_TIMER1_DUMP_ON_GOING,
	MD_EE_TIMER2_DUMP_ON_GOING,
};

enum {
	MD_EE_CASE_NORMAL = 0,
	MD_EE_CASE_ONLY_EX,
	MD_EE_CASE_ONLY_EX_OK,
	MD_EE_CASE_TX_TRG, // not using
	MD_EE_CASE_ISR_TRG, // not using
	MD_EE_CASE_NO_RESPONSE,
	MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG, // not using
	MD_EE_CASE_ONLY_SWINT,
	MD_EE_CASE_SWINT_MISSING,
	MD_EE_CASE_WDT,
};

typedef enum
{
  IPC_RPC_CPSVC_SECURE_ALGO_OP = 0x2001,
	IPC_RPC_GET_SECRO_OP		= 0x2002,
#ifdef CONFIG_MTK_TC1_FEATURE
	// LGE specific OP ID
	RPC_CCCI_LGE_FAC_READ_SIM_LOCK_TYPE = 0x3001,
	RPC_CCCI_LGE_FAC_READ_FUSG_FLAG,
	RPC_CCCI_LGE_FAC_CHECK_UNLOCK_CODE_VALIDNESS,
	RPC_CCCI_LGE_FAC_CHECK_NETWORK_CODE_VALIDNESS,
	RPC_CCCI_LGE_FAC_WRITE_SIM_LOCK_TYPE,
	RPC_CCCI_LGE_FAC_READ_IMEI,
	RPC_CCCI_LGE_FAC_WRITE_IMEI,
	RPC_CCCI_LGE_FAC_READ_NETWORK_CODE_LIST_NUM,
	RPC_CCCI_LGE_FAC_READ_NETWORK_CODE,
	//.............
	RPC_CCCI_LGE_FAC_WRITE_NETWORK_CODE_LIST_NUM,
	RPC_CCCI_LGE_FAC_WRITE_UNLOCK_CODE_VERIFY_FAIL_COUNT,
	RPC_CCCI_LGE_FAC_READ_UNLOCK_CODE_VERIFY_FAIL_COUNT,
	RPC_CCCI_LGE_FAC_WRITE_UNLOCK_FAIL_COUNT,
	RPC_CCCI_LGE_FAC_READ_UNLOCK_FAIL_COUNT,
	RPC_CCCI_LGE_FAC_WRITE_UNLOCK_CODE,
	RPC_CCCI_LGE_FAC_VERIFY_UNLOCK_CODE,
	RPC_CCCI_LGE_FAC_WRITE_NETWORK_CODE,
	RPC_CCCI_LGE_FAC_INIT_SIM_LOCK_DATA,
#endif
	IPC_RPC_GET_TDD_EINT_NUM_OP = 0x4001,
	IPC_RPC_GET_GPIO_NUM_OP		= 0x4002,
	IPC_RPC_GET_ADC_NUM_OP 		= 0x4003,
	IPC_RPC_GET_EMI_CLK_TYPE_OP = 0x4004,
	IPC_RPC_GET_EINT_ATTR_OP	= 0x4005,
	IPC_RPC_GET_GPIO_VAL_OP	    = 0x4006,
	IPC_RPC_GET_ADC_VAL_OP	    = 0x4007,
	IPC_RPC_GET_RF_CLK_BUF_OP   = 0x4008,
	IPC_RPC_GET_GPIO_ADC_OP	    = 0x4009,
	IPC_RPC_DSP_EMI_MPU_SETTING = 0x400B,

    IPC_RPC_IT_OP               = 0x4321,  
}RPC_OP_ID;

struct rpc_pkt {
	unsigned int len;
	void *buf;
} __attribute__ ((packed));

struct rpc_buffer {
	struct ccci_header header;
	u32 op_id;
	u32 para_num;
	u8  buffer[0];
} __attribute__ ((packed));

#define CLKBUF_MAX_COUNT 4 // hardcode, becarefull with data size, should not exceed tmp_data[] in ccci_rpc_work_helper()
struct ccci_rpc_clkbuf_result {
	u16 CLKBuf_Count;
	u8 CLKBuf_Status[CLKBUF_MAX_COUNT];
	u8 CLKBuf_SWCtrl_Status[CLKBUF_MAX_COUNT];
} __attribute__ ((packed)); // the total size should sync with tmp_data[] using in ccci_rpc_work_helper()

#ifdef CONFIG_MTK_TC1_FEATURE
#define GPIO_MAX_COUNT 6 // hardcode, becarefull with data size, should not exceed tmp_data[] in ccci_rpc_work_helper()
#else
#define GPIO_MAX_COUNT 3 // hardcode, becarefull with data size, should not exceed tmp_data[] in ccci_rpc_work_helper()
#endif
#define GPIO_PIN_NAME_STR_MAX_LEN 34
#define ADC_CH_NAME_STR_MAX_LEN 33

enum {
	RPC_REQ_GPIO_PIN = (1<<0),
	RPC_REQ_GPIO_VALUE = (1<<1),
	RPC_REQ_ADC_PIN = (1<<4),
	RPC_REQ_ADC_VALUE = (1<<5),
};
struct ccci_rpc_gpio_adc_intput {
	u8 reqMask;
	u8 gpioValidPinMask;
	char gpioPinName[GPIO_MAX_COUNT][GPIO_PIN_NAME_STR_MAX_LEN];
	u32 gpioPinNum[GPIO_MAX_COUNT];
	char adcChName[ADC_CH_NAME_STR_MAX_LEN];
	u32 adcChNum;
	u32 adcChMeasCount;
} __attribute__ ((packed));

struct ccci_rpc_gpio_adc_output {
	u32 gpioPinNum[GPIO_MAX_COUNT];
	u32 gpioPinValue[GPIO_MAX_COUNT];
	u32 adcChNum;
	u32 adcChMeasSum;
} __attribute__ ((packed)); // the total size should sync with tmp_data[] using in ccci_rpc_work_helper()

struct ccci_rpc_dsp_emi_mpu_input {
	u32 request;
} __attribute__ ((packed));

#define RPC_REQ_BUFFER_NUM       2 /* support 2 concurrently request*/
#define RPC_MAX_ARG_NUM          6 /* parameter number */
#define RPC_MAX_BUF_SIZE         2048 
#define RPC_API_RESP_ID          0xFFFF0000

#define FS_NO_ERROR										 0
#define FS_NO_OP										-1
#define	FS_PARAM_ERROR									-2
#define FS_NO_FEATURE									-3
#define FS_NO_MATCH									    -4
#define FS_FUNC_FAIL								    -5
#define FS_ERROR_RESERVED								-6
#define FS_MEM_OVERFLOW									-7

#define CCCI_SED_LEN_BYTES   16 
typedef struct {unsigned char sed[CCCI_SED_LEN_BYTES]; }sed_t;
#define SED_INITIALIZER { {[0 ... CCCI_SED_LEN_BYTES-1]=0}}

#endif //__PORT_KERNEL_H__
