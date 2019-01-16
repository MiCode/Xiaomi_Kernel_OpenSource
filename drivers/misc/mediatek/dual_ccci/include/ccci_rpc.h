/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccci_rpc.h
 *
 * Project:
 * --------
 *   YuSu
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

#ifndef __CCCI_RPC_H__
#define __CCCI_RPC_H__

#define CCCI_SED_LEN_BYTES   16 
typedef struct {unsigned char sed[CCCI_SED_LEN_BYTES]; }sed_t;
#define SED_INITIALIZER { {[0 ... CCCI_SED_LEN_BYTES-1]=0}}
/*******************************************************************************
 * Define marco or constant.
 *******************************************************************************/
#define IPC_RPC_EXCEPT_MAX_RETRY     7
#define IPC_RPC_MAX_RETRY            0xFFFF
#define IPC_RPC_MAX_ARG_NUM          6 /* parameter number */

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
    IPC_RPC_GET_GPIO_VAL_OP        = 0x4006,
    IPC_RPC_GET_ADC_VAL_OP        = 0x4007,
}RPC_OP_ID;

typedef struct
{
   unsigned int len;
   void *buf;
}RPC_PKT;

typedef struct
{
    unsigned int     op_id;
    unsigned char    buf[0];
}RPC_BUF;

#define FS_NO_ERROR                                         0
#define FS_NO_OP                                        -1
#define    FS_PARAM_ERROR                                    -2
#define FS_NO_FEATURE                                    -3
#define FS_NO_MATCH                                        -4
#define FS_FUNC_FAIL                                    -5
#define FS_ERROR_RESERVED                                -6
#define FS_MEM_OVERFLOW                                    -7

extern int ccci_rpc_init(int);
extern void ccci_rpc_exit(int);


#endif // __CCCI_RPC_H__
