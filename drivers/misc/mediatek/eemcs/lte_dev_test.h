#ifndef __LTE_DEV_H__
#define __LTE_DEV_H__

#include <linux/version.h>
#include <linux/tty.h>
#include <linux/skbuff.h>
#include <linux/string.h>
#include <linux/mmc/sdio_func.h>

#include "lte_df_main.h"
#include "eemcs_kal.h"


 struct mtlte_ttydev {
	int minor ;
	unsigned int open_count ;
	struct tty_port port ;
	struct tty_struct *tty_ref;
	volatile int throttle ;
};


/*************************************************************
*
*				SDIO  DEVICE  PART 
*
*************************************************************/
typedef enum {
	HDRV_SDBUS_BUSWIDTH_1BIT = 0x00,
	HDRV_SDBUS_BUSWIDTH_4BIT = 0x02,	
	HDRV_SDBUS_BUSWIDTH_BITMASK = 0x03,
} HDRV_SDBUS_BUSWIDTH, *PHDRV_SDBUS_BUSWIDTH ;

typedef enum {
	HDRV_SDBUS_FUNCTION_NUMBER=0,
	HDRV_SDBUS_FUNCTION_TYPE,
	HDRV_SDBUS_BUS_DRIVER_VERSION,
	HDRV_SDBUS_BUS_WIDTH,
	HDRV_SDBUS_BUS_CLOCK,
	HDRV_SDBUS_BUS_INTERFACE_CONTROL,
	HDRV_SDBUS_HOST_BLOCK_LENGTH,
	HDRV_SDBUS_FUNCTION_BLOCK_LENGTH,
	HDRV_SDBUS_FN0_BLOCK_LENGTH,
	HDRV_SDBUS_FUNCTION_INT_ENABLE
} HDRV_SDBUS_PROPERTY, *PHDRV_SDBUS_PROPERTY;

typedef enum _SDIO_THREAD_STATE {
	SDIO_THREAD_IDLE = 0x0 , 
	SDIO_THREAD_RUNNING = 0x1 , 

}SDIO_THREAD_STATE ;
/** 
 * @name: SDIO Function 0 control registers 
 */
#define SDIO_FN0_CCCR_CSRR       0x0000
#define SDIO_FN0_CCCR_SDSRR      0x0001
#define SDIO_FN0_CCCR_IOER       0x0002
#define SDIO_FN0_CCCR_IORR       0x0003
#define SDIO_FN0_CCCR_IER        0x0004
#define SDIO_FN0_CCCR_IPR        0x0005
#define SDIO_FN0_CCCR_IOAR       0x0006
#define SDIO_FN0_CCCR_BICR       0x0007
#define SDIO_FN0_CCCR_CCR        0x0008
#define SDIO_FN0_CCCR_CCPR       0x0009  // 0x0009 - 0x000B
#define SDIO_FN0_CCCR_BSR        0x000C
#define SDIO_FN0_CCCR_FSR        0x000D
#define SDIO_FN0_CCCR_EFR        0x000E
#define SDIO_FN0_CCCR_RFR        0x000F
#define SDIO_FN0_CCCR_F0BSR      0x0010	// 0x0010 - 0x0011
#define SDIO_FN0_CCCR_PCR        0x0012
#define SDIO_FN0_CCCR_HSR        0x0013
#define SDIO_FN0_CCCR_UHSR       0x0014
#define SDIO_FN0_CCCR_DSR        0x0015
#define SDIO_FN0_CCCR_IEXTR      0x0016
#define SDIO_FN0_CCCR_IOBSF1R    0x0110
#define SDIO_FN0_CCCR_CIS0       0x1000
#define SDIO_FN0_CCCR_CIS1       0x2000

#define SDIO_CIS0_FW_ADDR        0xBF27C000
#define SDIO_CIS1_FW_ADDR        0xBF27C400
#define CARD_CAPABILITY_ADDR     0xBF27CC00


/** 
 * @name: SDIO Function 1 control registers 
 */
#define SDIO_FN1_FBR_CSAR        0x0100
#define SDIO_FN1_FBR_EXTSCR      0x0101
#define SDIO_FN1_FBR_PSR         0x0102
#define SDIO_FN1_FBR_CIS         0x0109 // 0x109 ~ 0x10B
#define SDIO_FN1_FBR_F1BSR       0x0110 // 0x110 ~ 0x111

/*!
	SDIO Devie Function
 */
int sdio_func0_wr(unsigned int u4Register,unsigned char *pValue, unsigned Length) ;

int sdio_func0_rd(unsigned int u4Register,unsigned char *pValue, unsigned int Length) ;

int sdio_func1_wr(unsigned int u4Register,void *pBuffer, unsigned int Length) ;

int sdio_func1_rd(unsigned int u4Register,void *pBuffer, unsigned int Length) ;

int sdio_property_set(HDRV_SDBUS_PROPERTY PropFunc, unsigned char * pData, unsigned int size) ;

int sdio_property_get(HDRV_SDBUS_PROPERTY PropFunc, unsigned char * pData, unsigned int size) ;

int sdio_open_device(struct sdio_func *sdiofunc) ;

int sdio_close_device(struct sdio_func *sdiofunc) ;

#ifdef USER_BUILD_KERNEL
typedef int (*MTLTE_HAL_TO_HIF_CALLBACK)(int data);
int mtlte_hal_register_MSDC_ERR_callback(MTLTE_HAL_TO_HIF_CALLBACK func_ptr); 
#endif

/*************************************************************
*
*				LTE  DEVICE  PART 
*
*************************************************************/
struct mtlte_dev {
	/*NETDEV related structure member*/	
	//struct net_device *net_dev[LTE_NET_MAX_INDEX] ;
	//char name[32];

/*TTY related structure member*/		
	//struct tty_struct *tty_dev[LTE_TTY_MAX_DEVNUM] ;
	//unsigned long flags;		/* various conditions */
			
#ifdef CONFIG_REQUEST_FW
	const struct firmware *fw;
#endif
	const char *fw_name;
	
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22))
	struct net_device_stats net_stats;
#endif

/*SDIO/HAL related structure member*/	
	struct sdio_func *sdio_func;	/* the SDIO Device Function Core*/
	unsigned card_exist ;

	struct task_struct *sdio_thread;	
	wait_queue_head_t sdio_thread_wq;		
	volatile int sdio_thread_kick;
    volatile int sdio_thread_kick_isr;
    volatile int sdio_thread_kick_own_timer;
	volatile SDIO_THREAD_STATE sdio_thread_state ;

	struct kref kref;
	KAL_MUTEX   thread_kick_lock ;
};

/*************************************************************
*
*				TEST  DEVICE  PART 
*
*************************************************************/
#if TEST_DRV
#define LTE_TEST_DEVICE_MINOR	0

#define H2D_INT_H2DMB_init_req   (0x1<<31)
#define H2D_INT_H2DMB_data_sent  (0x1<<30)
#define H2D_INT_D2HMB_init_ack   (0x1<<29)
#define H2D_INT_D2HMB_data_ack   (0x1<<28)

#define D2H_INT_H2DMB_init_ack  (0x1<<31)
#define D2H_INT_H2DMB_data_ack  (0x1<<30)
#define D2H_INT_D2HMB_init_req  (0x1<<29)
#define D2H_INT_D2HMB_data_sent (0x1<<28)


typedef struct _at_msg_status {
    bool D2H_INT_H2DMB_init_ack_st;
    bool D2H_INT_H2DMB_data_ack_st;
    bool D2H_NEW_MSD_receiving_st;
    bool D2H_NEW_MSG_arrived;
} at_msg_status;


int mtlte_dev_test_probe(int index, struct device *dev);

void mtlte_dev_test_detach(int index);

int mtlte_dev_test_drvinit(void);

int mtlte_dev_test_drvdeinit(void) ;



typedef enum _athif_cmd_code{
    /*
     *  @brief set the hif at dataflow task reload RGPD mode
     */
	ATHIF_CMD_SET_RGPD_RL_MD = 0,
    /*!
     *  @brief set the at dataflow task forward mode
     */
	ATHIF_CMD_SET_FWD_MD,
    /*!
     *  @brief set the at dataflow task loopback mode type (direct / random / ..., etc.)
     */
	ATHIF_CMD_SET_LB_TYPE,
    /*!
     *  @brief get the received or sent packet count from the data flow driver
     */
	ATHIF_CMD_GET_PKT_CNT,	
    /*!
     *  @brief Test the DL only performance and the performance calculated on host
     */
	ATHIF_CMD_DL_PERF,	
    /*!
     *  @brief host send this cmd with pattern in param buffer and device send back in the CMD_ACK
     */
	ATHIF_CMD_CTRL_CH_TST,	
    /*!
     *  @brief device would send the specific GPD format packet to host on specific queue
     */
	ATHIF_CMD_DL_SEND_N,	
    /*!
     *  @brief device would send N random GPD format packet to host on specific queue
     */
	ATHIF_CMD_DL_SEND_RAND_N,
    /*!
     *  @brief device would send random GPD format packet stress for specific loop
     */
	ATHIF_CMD_DL_SEND_RAND_STRESS,
    /*!
     *  @brief device would shut the HIF port and then re-attach
     */
	ATHIF_CMD_HIF_RECONNECT,
    /*!
     *  @brief pause or resume dataflow RGPD reload handle
     */
	ATHIF_CMD_PAUSE_RGPD_RL,
    /*!
     *  @brief prepare RGPD with specific format and enqueue for unit test
     */
	ATHIF_CMD_PREPARE_RGPD,
    /*!
     *  @brief configure the RGPD format for reload buffer thread
     */
	ATHIF_CMD_SET_RGPD_FROMAT,
    /*!
     *  @brief configure the TGPD format for loopback
     */
	ATHIF_CMD_SET_TGPD_FROMAT,
    /*!
     *  @brief write specific memory
     */
	ATHIF_CMD_WRITE_MEM,
    /*!
     *  @brief read specific memory
     */
	ATHIF_CMD_READ_MEM,	
    /*!
     *  @brief get current qmu interrupt handle recorded info
     */
	ATHIF_CMD_GET_QMU_INTR_INFO,
    /*!
     *  @brief clear current qmu interrupt handle recorded info
     */
	ATHIF_CMD_CLR_QMU_INTR_INFO,
    /*!
     *  @brief prepare n RGPD and specifiy which RGPD IOC assert
     */
	ATHIF_CMD_PREPARE_IOC_RGPD,
    /*!
     *  @brief prepare n TGPD and specify which TGPD IOC assert
     */
	ATHIF_CMD_PREPARE_IOC_TGPD,
    /*!
     *  @brief configure new checksum length
     */
	ATHIF_CMD_SET_CS_LEN,
    /*!
     *  @brief pause or resume the tx and rx dataflow reload queue dequeue flow
     */
	ATHIF_CMD_PAUSE_RESUME_DATAFLOW,
    /*!
     *  @brief prepare RGPD and specifiy which RGPD CS setting
     */
	ATHIF_CMD_PREPARE_CS_TST_RGPD,
    /*!
     *  @brief compare and free the local RGPD, and send the compare/free result
     */
	ATHIF_CMD_GET_LOCAL_RGPD_RSLT,
    /*!
     *  @brief prepare TGPD and specifiy which TGPD CS setting
     */
	ATHIF_CMD_PREPARE_CS_TST_TGPD,
    /*!
     *  @brief check and free the local TGPD 
     */
	ATHIF_CMD_GET_LOCAL_TGPD_RSLT,
    /*!
     *  @brief set GPD buffer offset
     */
	ATHIF_CMD_SET_BUF_OFFSET,
    /*!
     *  @brief set configure USB EP0 DMA or PIO mode
     */
	ATHIF_CMD_USB_SET_EP0_DMA,
    /*!
     *  @brief set timing to stop queue
     */
	ATHIF_CMD_SET_STOPQ_TIME,
    /*!
     *  @brief configure the dataflow reload specific rgpd type
     */
	ATHIF_CMD_CFG_SPECIFIC_RGPD_RL,
    /*!
     *  @brief host request to prepare MSD RGPD with specific MSD xfer length
     */
	ATHIF_CMD_PREPARE_MSD_RGPD,
    /*!
     *  @brief host request to change the loopback queue mapping
     */
	ATHIF_CMD_SET_LB_QUE_MAP_TBL,
    /*!
     *  @brief get current test HW IP information for the difference of each IP
     */
	ATHIF_CMD_GET_USB_IP_INFO,
    /*!
     *  @brief set the USB IP max transfer speed
     */
    ATHIF_CMD_SET_USB_SPEED,
	/*
	 * 	@brief	reconfigure the USB endpoint FIFO,TYPE...
	*/
	ATHIF_CMD_RECONFIG_USB_EP,
	/*
	 * 	@brief	emulate the exception handle to test hif exception handle driver
	*/
	ATHIF_CMD_EXCEPTION_DRV_TST,
	/*
	 * 	@brief	emulate the exception handle swithc USB channel
	*/
    ATHIF_CMD_EXCEPTION_SWITCH_USB_CH_TST,
	/*
	 * 	@brief	get the USB event counter information from usb_at.c
	*/
    ATHIF_CMD_GET_USB_EVT_CNT,
	/*
	 * 	@brief	Waiting expected delay time to issue remote wakeup after suspend event
	 *			This is a non-ack command
	*/
    ATHIF_CMD_USB_REMOTE_WK_TST,
    
	/*
	 * 	@brief	Used to test the USB3.0 device notify TP
	*/
    ATHIF_CMD_U3_DEV_NOTIFY,

	/*
	 * 	@brief	device request Ux and device request exit tst
	*/
    ATHIF_CMD_U3_DEV_REQ_UX_TST,

	/*
	 * 	@brief	set USB3.0 P1/P2 wakeup device event for loopback test
	*/
    ATHIF_CMD_U3_LB_DEV_UX_EXIT_TYPE,

	/*
	 * 	@brief	device issue LPM remote wakeup and DL packet, no ack
	*/
    ATHIF_CMD_U2_LPM_REMOTE_WK_TST,

	/*
	 * 	@brief	device issue LPM remote wakeup and DL packet, no ack
	*/
    ATHIF_CMD_U2_LPM_REMOTE_WK_RAND_TST,

	/*
	 * 	@brief	host issue suspend and device delay for expected time to issue remote wakeup or not
	*/
    ATHIF_CMD_SEL_SUSPEND_TST,
	/*
	 * 	@brief	used to test MT7208 Fabric MD/AP own
	*/
    ATHIF_CMD_AP_MD_OWN_TST,

	/*
	 * 	@brief	watch dog and sw reset
	*/
    ATHIF_CMD_SET_WDT,

	/*
	 * 	@brief	USB sw reset
	*/
    ATHIF_CMD_SET_USB_SWRST,

	/*
	 * 	@brief	USB sleep mode wakeup event unit test
	*/
    ATHIF_CMD_USB_ENTER_SM,

	/*
	 * 	@brief	Get SM information and could configure to clear information
	*/
    ATHIF_CMD_GET_CLR_SM_INFO,

	/*
	 * 	@brief	enable automatically entry sleep mode
	*/
    ATHIF_CMD_EN_AUTO_SLEEP_MD,

	/*
	 * 	@brief	enable sleep mode loopback tx packet delay
	*/
    ATHIF_CMD_EN_SM_LB_TX_DELAY,



/* **************************** */
/*         Below is SDIO part               */
/* **************************** */
  /*!
   *  @brief  start the auto test cmd loopback test of sdio
   */
    SDIO_AT_CMD_LOOPBACK = 0x100,

  /*!
   *  @brief  start the mailbox loopback test of sdio
   */
    SDIO_AT_MB_LOOPBACK,

  /*!
   *  @brief  start the auto test sw interrupt test of sdio
   */
    SDIO_AT_SW_INT,
    
 /*!
   *  @brief  start the auto test of D2H normal operation releated interrupt
   */
    SDIO_AT_D2H_NORMAL_OP,
    
 /*!
   *  @brief  start the auto test of device self normal opearation interrupt
   */
    SDIO_AT_SELF_NORMAL_INT,

 /*!
   *  @brief  start the auto test of device self normal opearation interrupt
   */
    SDIO_AT_NORMAL_DLQ_OP,

 /*!
   *  @brief  send spaciel DL GPD for test
   */
    SDIO_AT_DL_SEND_SP,

  /*!
   *  @brief  start, end big size packet test
   */
    SDIO_AT_BIG_SIZE,

  /*!
   *  @brief  UL big size packet (up to 4K) test
   */
    SDIO_AT_UL_BIG_SIZE,

 /*!
   *  @brief  UL big size packet (up to 64K) test
   */
    SDIO_AT_DL_BIG_SIZE, 
    
 /*!
   *  @brief  set whether ISR is work at normal mode or test mode.
   */
   SDIO_AT_DL_INT_TEST_SWITCH,
 /*!
  *  @brief  Reset & flush the DL queue for error handle test.
  */
   SDIO_AT_RESET_DL_QUEUE,

 /*!
  *  @brief  Reset & flush the UL queue for error handle test.
  */
    SDIO_AT_RESET_UL_QUEUE,

  /*!
  *  @brief  read the SDIO interrupt status.
  */
   SDIO_AT_READ_INT_STATUS,

  /*!
  *  @brief  set the SDIO interrupt mask value.
  */
    SDIO_AT_SET_INT_MASK,

 /*!
  *  @brief  read the SDIO interrupt mask value.
  */
    SDIO_AT_READ_INT_MASK,

 /*!
  *  @brief  read the SDIO Tx data compare result.
  */
    SDIO_AT_READ_TX_COMP_RET,

 /*!
  *  @brief  read the SDIO Tx data compare result.
  */
    SDIO_AT_WD_RESET,

   /*!
  *  @brief  Self Sleep and Wake-up test of SDIO.
  */
    SDIO_AT_SELF_SLEEP,
  
    /*!
  *  @brief  Set wakeup eint pin which will used at formal solution.
  */
    SDIO_AT_SET_WAKEUP_EINT,
  
     /*!
  *  @brief  Test the wakeup event of SDIO.
  */
    SDIO_AT_WAKEUP_EVENT_TEST,
  
  /*!
  *  @brief  Clean 18v bit for re-initialize SDIO 3.0.
  */
    SDIO_AT_CLEAN_18V_BIT = 0x1F0,
    
    /*!
  *  @brief  set a sw interrupt to test block interrupt.
  */
    SDIO_AT_SET_BLOCK_INT = 0x1F1,

    /*!
  *  @brief  set pre SLT test end event.
  */
    SDIO_AT_PRESLT_END = 0x1F2,
    
    /*!
  *  @brief  Do exception test.
  */
    SDIO_AT_EXCEPTION_TEST = 0x1F3,


    
}athif_cmd_code_e;


typedef struct _athif_init_cmd {
    /*!
	 *	@brief	the signature
 	*/
    char signature[2];
    
    /*!
	 *	@brief	the command set length include the header and payload
 	*/
    short length;
    
}athif_init_cmd_t;


typedef struct _athif_cmd {
	/*!
	 *	@brief	the signature
 	*/
	kal_uint8 	signature[2];
	/*!
	 *	@brief	the command set length include the header and payload
 	*/
	kal_uint16 	len;
	/*!
	 *	@brief	the command code
 	*/
	kal_uint16 	cmd;
	/*!
	 *	@brief	reserve for 4byte align
 	*/
	kal_uint8	reverve[2];
	/*!
	 *	@brief	the command parameter
 	*/
	kal_uint8 	buf[512];
}athif_cmd_t;

typedef struct _athif_status {
	/*!
	 *	@brief	the signature
 	*/
	kal_uint8 	signature[2];
	/*!
	 *	@brief	the status length include the header and payload
 	*/
	kal_uint16 	len;
	/*!
	 *	@brief	the status code
 	*/
	kal_uint16 	status;
	/*!
	 *	@brief	reserve for 4byte align
 	*/
	kal_uint8	reverve[2];
	/*!
	 *	@brief	the status detail statement
 	*/
	kal_uint8 	buf[512];
}athif_status_t;



#endif

#endif
