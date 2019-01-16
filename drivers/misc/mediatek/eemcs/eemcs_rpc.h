/*****************************************************************************
 *
 * Filename:
 * ---------
 *   eemcs_rpc.h
 *
 * Project:
 * --------
 *   
 *
 * Description:
 * ------------
 *   
 *
 * Author:
 * -------
 *   
 *
 ****************************************************************************/

#ifndef __EEMCS_RPC_H
#define __EEMCS_RPC_H


//#include <crypto_engine_export.h>
//#include <sec_error.h>
#include <linux/skbuff.h>

#include "eemcs_ccci.h"
#include "eemcs_kal.h"
#ifdef _EEMCS_RPC_UT 
#include "eemcs_rpc_ut.h"
#endif

#define CCCI_SED_LEN_BYTES   16 
typedef struct {unsigned char sed[CCCI_SED_LEN_BYTES]; }sed_t;
#define SED_INITIALIZER { {[0 ... CCCI_SED_LEN_BYTES-1]=0}}
/*******************************************************************************
 * Define marco or constant.
 *******************************************************************************/
#define IPC_RPC_EXCEPT_MAX_RETRY     7
#define IPC_RPC_MAX_RETRY            (0xFFFF)
#define IPC_RPC_REQ_BUFFER_NUM       2 /* support 2 concurrently request*/
#define IPC_RPC_MAX_ARG_NUM          6 /* parameter number */
#define IPC_RPC_MAX_BUF_SIZE         2048 

#define IPC_RPC_USE_DEFAULT_INDEX    -1
#define IPC_RPC_API_RESP_ID          0xFFFF0000
#define IPC_RPC_INC_BUF_INDEX(x)     (x = (x + 1) % IPC_RPC_REQ_BUFFER_NUM)

/*******************************************************************************
 * Define data structure.
 *******************************************************************************/
typedef enum
{
    IPC_RPC_CPSVC_SECURE_ALGO_OP = 0x2001,
    IPC_RPC_GET_SECRO_OP        = 0x2002,
    IPC_RPC_GET_TDD_EINT_NUM_OP = 0x4001,
    IPC_RPC_GET_TDD_GPIO_NUM_OP = 0x4002,
    IPC_RPC_GET_TDD_ADC_NUM_OP  = 0x4003,
    IPC_RPC_GET_EMI_CLK_TYPE_OP = 0x4004,
    IPC_RPC_GET_EINT_ATTR_OP    = 0x4005,

    IPC_RPC_UT_OP               = 0x1234,  
    IPC_RPC_IT_OP               = 0x4321,  
}RPC_OP_ID;

typedef struct
{
   unsigned int len;
   void *buf;
}RPC_PKT;

typedef struct
{
    unsigned int     op_id;
    unsigned char    buf[IPC_RPC_MAX_BUF_SIZE];
}RPC_BUF;

typedef struct IPC_RPC_StreamBuffer_STRUCT
{
    CCCI_BUFF_T   ccci_header;
    kal_uint32    rpc_opid;
	kal_uint32	  num_para;
	kal_uint8     buffer[IPC_RPC_MAX_BUF_SIZE];
	
} IPC_RPC_StreamBuffer_T;
typedef IPC_RPC_StreamBuffer_T* pIPC_RPC_StreamBuffer_T;

#if 0 
/* Struct to define the control channel of RPC service */
typedef struct CCCI_RPC_STRUCT
{
    kal_uint32 send_channel;            /* RPC channel for TX */
    kal_uint32 receive_channel;         /* RPC channel for RX */
    /* <CCCI released APIs> */
    kal_int32  (*ccci_write_gpd)(CCCI_CHANNEL_T channel, ccci_io_request_t *p_ccci_DL_ior, ccci_io_ext_info_t* pextinfo);

    kal_int32  (*ccci_init_gpdior)(CCCI_CHANNEL_T channel, CCCI_IORCALLBACK ior_funp);
    kal_int32  (*ccci_deinit)(CCCI_CHANNEL_T channel);
	kal_int32  (*ccci_rpc_polling_io)(CCCI_CHANNEL_T channel,qbm_gpd * p_gpd,kal_bool is_tx);

	kal_bool   (*check_kal_systemInit)(void);

    kal_uint32 ut_flag;

    qbm_gpd *p_polling_gpd;             /* polling mode GPD */
    kal_uint32 allocated_gpd_number;

} CCCI_RPC_T;

#endif

#define FS_NO_ERROR                                      0
#define FS_NO_OP                                        -1
#define FS_PARAM_ERROR                                  -2
#define FS_NO_FEATURE                                   -3
#define FS_NO_MATCH                                     -4
#define FS_FUNC_FAIL                                    -5
#define FS_ERROR_RESERVED                               -6
#define FS_MEM_OVERFLOW                                 -7

/* EMCS opeartion Error. */
//  I for Internal use, O for device status, should cover by error handling..
#define EMCS_ERR_NONE           0
#define EMCS_ERR_TIMEOUT        20 /*[O] wait Interrupt or device read fail*/
#define EMCS_ERR_BT_STATUS      21 /*[I] invalid MD Status.*/
#define EMCS_ERR_CMDCRC         22 /*[O] invalid command*/
#define EMCS_ERR_LOAD_BIN       23 /*[O] MD BIN file open fail*/
#define EMCS_ERR_MSG_OVERFLOW   24 /*[O] receive message to long*/
#define EMCS_ERR_PKT_OVERFLOW   25 /*[I] send package big the tx limitaion*/
#define EMCS_ERR_INVALID_PARA   26 /*[I] emcs driver parameter check fail*/
#define EMCS_ERR_GET_OWNER      27 /*[O] get device ownership fail*/     
#define EMCS_ERR_NOMEM          28
#define EMCS_ERR_NOINIT         29
#define EMCS_ERR_INVAL_PARA     30
#define EMCS_ERR_TX_FAIL        31
#define EMCS_ERR_RX_FAIL        32
#define EMCS_ERROR_BUSY         33
#define EMCS_ERROR_NODEV        34


/* IPC Instance structure define */
typedef struct _eemcs_rpc_inst_t
{
    ccci_ch_set         ccci_ch;
}eemcs_rpc_inst_t;

#ifdef CCCI_SDIO_HEAD
#define CCCI_RPC_HEADER_ROOM                          (sizeof(SDIO_H)+sizeof(CCCI_BUFF_T))
#else
#define CCCI_RPC_HEADER_ROOM                          (sizeof(CCCI_BUFF_T))
#endif

#define RPC_UT_SUCCESS									1
#define RPC_UT_FAIL                                    -1 
#define RPC_IT_SUCCESS									1
#define RPC_IT_FAIL                                    -1 

#define ut_ret_val                                     0xa5a5
#define it_ret_val                                     0xa5a5

#ifdef _EEMCS_RPC_UT
  #define ccci_rpc_ch_write_desc_to_q(ch_num,desc_p)     eemcs_rpc_ut_UL_write_skb_to_swq(ch_num, desc_p) 
#else
  #define ccci_rpc_ch_write_desc_to_q(ch_num,desc_p)     eemcs_ccci_UL_write_skb_to_swq(ch_num, desc_p)
#endif
#define ccci_ch_register(ch_num,cb,para)		   eemcs_ccci_register_callback(ch_num,cb,para) 
#define ccci_ch_unregister(ch_num)				   eemcs_ccci_unregister_callback(ch_num) 
#define ccci_ch_write_space_alloc(ch_num)		   eemcs_ccci_UL_write_room_alloc(ch_num)

#define ccci_rpc_mem_alloc(sz, flag)               __dev_alloc_skb(sz, flag)


extern KAL_INT32 eemcs_rpc_callback(struct sk_buff *skb, KAL_UINT32 private_data);
extern int eemcs_rpc_mod_init(void);
extern void eemcs_rpc_exit(void);

#endif
