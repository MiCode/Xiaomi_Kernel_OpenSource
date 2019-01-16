#ifndef __SDIOAT_DEF_H__
#define __SDIOAT_DEF_H__

#define kal_uint8 	unsigned char
#define kal_uint16 	unsigned short
#define kal_uint32	unsigned int
#define kal_bool	unsigned char


/*!
 *  @brief 	The max txq/rxq number of SDIO
 *			The MAX SDIO UL queue num will be increase to 8 in future. 
*/

#define HIF_MAX_ULQ_NUM		7
#define HIF_MAX_DLQ_NUM		4

#define MAX_UL_PKT_SIZE     2048

typedef struct _AT_PKT_HEADER {
	kal_uint8	SrcQID:4;
	kal_uint8	DstQID:4;
	kal_uint8	SeqNo;
	kal_uint8	RndSeed;
	kal_uint8	Checksum;
	kal_uint32	PktLen;
	kal_uint8	Data[0];
} AT_PKT_HEADER, *PAT_PKT_HEADER;


typedef struct _len_range{
	unsigned int start_len;
	unsigned int end_len;
}len_range_t;


#define ATHIF_CMD_SET_SIG	"CS"
#define ATHIF_STOP_SIG		"ST"
#define ATHIF_CMD_ACK_SIG	"CA"
#define ATHIF_GET_RSLT_SIG	"GR"

typedef enum _athif_ack_st{
	AT_CMD_ACK_READY_SUCCESS = 0 ,
	AT_CMD_ACK_BUSY,
	AT_CMD_ACK_FAIL,
}athif_ack_st_e;


typedef enum _athif_req {
	AT_CMD_SET = 0,
	AT_CMD_ACK,
	AT_GET_RESULT,
	AT_STOP,
} athif_req_e;

/*the length of the command and statushead*/
#define ATHIF_CMD_HD_LEN		8
#define ATHIF_STATUS_HD_LEN		8


/*!
 *  @brief 	the reload RGPD format mode
 *  @param ATHIF_RGPD_BASIC		allocate QBM_TYPE_GPD_BD_TST with GPD only
 *  @param ATHIF_RGPD_RANDOM	prepare the random RGPD format, (without BD, with N BD)
 *  @param ATHIF_RGPD_SPECIFIC	prepare the GPD with specific GPD format configured by host
 *  @param ATHIF_RGPD_ACM		prepare the RGPD with the real system ACM used type
 *  @param ATHIF_RGPD_ECM		prepare the RGPD with the real system ECM used type
 *  @param ATHIF_RGPD_MSD		prepare the RGPD with the real system MSD used type
 *  @param ATHIF_RGPD_BIGSIZE	prepare the RGPD with the big size (can be used to test UL big size pkt)
 */
typedef enum _athif_rld_rgpd_md{
    ATHIF_RGPD_BASIC = 0,
    ATHIF_RGPD_RANDOM,
    ATHIF_RGPD_RANDOM_W_BPS,
    ATHIF_RGPD_RANDOM_W_BPS_IOC,
    ATHIF_RGPD_SPECIFIC,
    ATHIF_RGPD_BYPASS,
    ATHIF_RGPD_BUF_MISALIGN,
    ATHIF_RGPD_ACM,
    ATHIF_RGPD_ECM,
    ATHIF_RGPD_MSD,
    ATHIF_RGPD_TCM,
}athif_rld_rgpd_md_e;



/*!
 *  @brief the loopback mode
 *  @param ATHIF_LB_DIRECT		directly forwarding the RGPD to the TXQ (high performance)
 *  @param ATHIF_LB_RANDOM		randomly generate the TGPD and copy the RGPD data to TGPD
 *  @param ATHIF_LB_SPECIFIC	generate the TGPD with specific format configured by host and copy the RGPD data to TGPD
 *  @param ATHIF_LB_ACM			emulate the ACM RX/TX transfer
 *  @param ATHIF_LB_ECM			emulate the ECM RX/TX transfer
 *  @param ATHIF_LB_MSD			emulate the MSD RX/TX transfer
 */
typedef enum _athif_lb_tgpd_md{
	ATHIF_LB_TGPD_DIRECT = 0,
	ATHIF_LB_TGPD_RANDOM,
	ATHIF_LB_TGPD_SPECIFIC,
	ATHIF_LB_TGPD_ACM,
	ATHIF_LB_TGPD_ECM,
	ATHIF_LB_TGPD_MSD,
	ATHIF_LB_TGPD_EMPTY_ENQ,
}athif_lb_tgpd_md_e;


/*!
 *  @brief the hif autotest RGPD forwarding mode
 *  @param ATHIF_FWD_FREE_ONLY		just free the RGPD after received
 *  @param ATHIF_FWD_CMP_DATA		compare the RGPD data with the pre-defined data pattern format
 *  @param ATHIF_FWD_LOOPBACK		forward the RGPD to the TGPD on specific TXQ
 */
typedef enum _athif_fwd_mode{
	ATHIF_FWD_FREE_ONLY = 0,
	ATHIF_FWD_CMP_DATA,
	ATHIF_FWD_LOOPBACK,
}athif_fwd_mode_e;


typedef enum _athif_gpd_type{
	ATHIF_GPD_GENERIC = 0,
	ATHIF_GPD_ACM,
	ATHIF_GPD_ECM,
	ATHIF_GPD_MSD,
}athif_gpd_type_e;

/*athif_cmd_dl_perf queue configuration*/
typedef struct _athif_dl_perf_que{
	kal_uint8			que_en;
	kal_uint8			gpd_type;
	kal_uint8			bd_num;
	kal_uint8			q_num;
	kal_uint32			gpd_num;
	kal_uint32			gpd_len;
	kal_uint32			pkt_cnt;
}athif_dl_perf_que_t;


/*
 *	@brief	define the constraint of auto test GPD format
*/
#define ATHIF_MAX_RBD_NUM	5
#define ATHIF_MAX_TBD_NUM	10


typedef struct _athif_dl_perf_cfg{
	athif_dl_perf_que_t	txq_cfg[HIF_MAX_ULQ_NUM];
}athif_dl_perf_cfg_t;

/*the RGPD format*/
typedef struct _athif_ul_rgpd_format{
	kal_uint32 rgpd_allow_len;
	kal_uint32 rbd_num;
	kal_uint32 rbd_allow_len[ATHIF_MAX_RBD_NUM];
}athif_ul_rgpd_format_t;


/*used for host to configure the test configure*/
typedef struct _athif_ul_rgpd_tst_cfg{
	kal_uint32 				q_num;
	kal_uint32 				gpd_num;
	athif_ul_rgpd_format_t	rgpd_format;
}athif_ul_rgpd_tst_cfg_t;

/*the TBD format*/
typedef struct _athif_dl_tbd_format{
	kal_uint32 tbd_buf_len;
	kal_uint32 tbd_ext_len;
}athif_dl_tbd_format_t;

/*the TGPD format*/
typedef struct _athif_dl_tgpd_format{
	kal_uint32 tgpd_buf_len;
	kal_uint32 tgpd_ext_len;
	kal_uint32 tbd_num;
	athif_dl_tbd_format_t tbd_format[ATHIF_MAX_TBD_NUM];
}athif_dl_tgpd_format_t;

typedef struct _athif_dl_tgpd_cfg{
	kal_uint32 q_num;
	kal_uint32 gpd_num;
	athif_dl_tgpd_format_t tgpd_format;
}athif_dl_tgpd_cfg_t;

typedef struct _athif_basic_set{
	kal_uint32 q_num;
	kal_uint32 gpd_num;
}athif_basic_set_t;

typedef struct _athif_mem_tst_cfg {
	kal_uint32 mem_addr;
	kal_uint32 len;
	kal_uint8 mem_val[0];
}athif_mem_tst_cfg_t;


typedef struct _athif_gpd_ioc_cfg{
	kal_uint32 	q_num;
	kal_uint32 	gpd_num;
	/*
	 *	@brief 	bit 0: ioc , 1:bps, 2:cs_16(0:cs_12,1:cs_16) , 
	 *			3: gpd_cs_err (0: correct , 1: wrong) , 4: bd_cs_err (0: correct, 1: wrong)
	*/
	kal_uint8	ioc_bps_valid[0]; 
}athif_gpd_ioc_cfg_t, athif_gpd_cs_cfg_t;

typedef struct _athif_local_rgpd_rslt{
	kal_uint32 free_cnt;
	kal_uint32 correct_cnt;
	kal_uint32 fail_cnt;
}athif_local_rgpd_rslt_t;


typedef struct _athif_local_tgpd_rslt{
	kal_uint32 free_cnt;
	kal_uint32 sent_cnt;
	kal_uint32 remain_cnt;//except tail
}athif_local_tgpd_rslt_t;

typedef struct _athif_stopq_tst_cfg{
	kal_uint32 t_tick; 	//timeout of msec to stop queue
	kal_uint16 is_tx;	//specific txq or rxq
	kal_uint16 q_num;	//N for queue N, and 0xFFFF for stop all queue
}athif_stopq_tst_cfg_t;

/*used for host to configure the MSD RGPD test configure*/
typedef struct _athif_msd_rgpd_tst_cfg{
	kal_uint32 q_num;
	kal_uint32 gpd_num;
	kal_uint32 msd_lba_num;				
}athif_msd_rgpd_tst_cfg_t;


typedef enum _rx_basic_tst_case {
 	ATCASE_DL_TGPD_NUM_BOUNDARY = 0x0,
 	ATCASE_DL_TGPD_LEN_BOUNDARY,
 	ATCASE_DL_TGPD_EXT_BOUNDARY,
 	ATCASE_DL_TBD_NUM_BOUNDARY,
 	ATCASE_DL_1TBD_LEN_BOUNDARY,
 	ATCASE_DL_2TBD_LEN_BOUNDARY,
 	ATCASE_DL_2TBD_EXT_BOUNDARY,
 	ATCASE_DL_RANDOM,
 	ATCASE_DL_RANDOM_STRESS,
 	ATCASE_DL_SPECIFIC,
 	ATCASE_DL_SDIO_SP,
}rx_basic_tst_case_e;


typedef enum _perf_tst_case {
	ATCASE_PERF_TX = 0,
	ATCASE_PERF_TX_DEV_CMP,
	ATCASE_PERF_RX,
	ATCASE_PERF_TXRX,
	ATCASE_PERF_TXRX_RAND,
	ATCASE_PERF_TXRX_ECM,
	ATCASE_PERF_UL_RAND,
	ATCASE_PERF_TXRX_ACM,
	ATCASE_PERF_TXRX_EMPTY_ENQ,
	ATCASE_PERF_TX_HW_LIMIT,
	ATCASE_PERF_TXRX_UNALIGN,
}perf_tst_case_e;



typedef struct _athif_auto_sm_cfg{
	kal_bool enable;
	kal_bool rtc_wk_en;
	kal_uint32 sleep_dur_ms; //0: random
	kal_uint32 rtc_wk_dur_sec; //0: random	
}athif_auto_sm_cfg_t;


/*************************************************************
*				SDIO SPECIAL  PART 
*************************************************************/

/*!
 *  @brief 	The max txq/rxq number of SDIO
 *			The MAX SDIO UL queue num will be increase to 8 in future. 
*/


typedef struct _athif_sdio_set_ulq_count{
	kal_uint32 q_num;
	kal_uint32 gpd_num;				
}athif_sdio_set_ulq_count_t;

typedef struct _athif_sdio_set_dlq_pkt{
	kal_uint32 q_num;
	kal_uint32 pkt_num;
    kal_uint32 pkt_len[16];
}athif_sdio_set_dlq_pkt_t;


typedef struct _hifsdio_isr_status{
	kal_uint32	GLO_INTR_Status;
	kal_uint16	SW_INTR_Status;
    kal_uint16	FW_INTR_Status;
    kal_uint32	UL0_INTR_Status;
    kal_uint32	DL0_INTR_Status;
    kal_uint32	DL1_INTR_Status;
    
} hifsdio_isr_status_t;

/*!
 *  @brief hifsdio_isr_mask is used to record the interrupt enable bitmap mask
 */
typedef struct _hifsdio_isr_en_mask{
	kal_uint32	GLO_INTR_En_Msk;
	kal_uint16	SW_INTR_En_Msk;
    kal_uint16	FW_INTR_En_Msk;
    kal_uint32	UL0_INTR_En_Msk;
    kal_uint32	DL0_INTR_En_Msk;
    kal_uint32	DL1_INTR_En_Msk;
    
} hifsdio_isr_en_mask_t;


typedef enum _hifsdio_isr_mask_code{
    GLO_INTR_Msk = 0,
    SW_INTR_Msk,
    FW_INTR_Msk,
    UL0_INTR_Msk,
    DL0_INTR_Msk,
    DL1_INTR_Msk,
    SET_ALL_Msk = 0xFF,
}hifsdio_isr_mask_e;


#endif

