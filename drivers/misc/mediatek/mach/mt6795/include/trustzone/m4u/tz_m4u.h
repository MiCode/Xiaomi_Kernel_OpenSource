/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
 * All rights reserved
 * 
 * The present software is the confidential and proprietary information of
 * TRUSTONIC LIMITED. You shall not disclose the present software and shall
 * use it only in accordance with the terms of the license agreement you
 * entered into with TRUSTONIC LIMITED. This software may be subject to
 * export or import laws in certain countries.
 */

#ifndef __TZ_M4U_H__
#define __TZ_M4U_H__

//#include "drStd.h"
#include "m4u_port.h"

#if defined(__cplusplus)
extern "C" {
#endif

//#define __M4U_SECURE_SYSTRACE_ENABLE__

#define M4U_NONSEC_MVA_START (0x20000000)

#define CMD_M4U_MAGIC           (0x77880000)

typedef enum {
    CMD_M4U_ADD = CMD_M4U_MAGIC,
    CMD_M4U_CFG_PORT,
    CMD_M4U_MAP_NONSEC_BUFFER,
    CMD_M4U_SEC_INIT,
    CMD_M4U_ALLOC_MVA,
    CMD_M4U_DEALLOC_MVA,
    CMD_M4U_REG_BACKUP,
    CMD_M4U_REG_RESTORE,
    CMD_M4U_PRINT_PORT,
    CMD_M4U_DUMP_SEC_PGTABLE,

    CMD_M4UTL_INIT,

    CMD_M4U_OPEN_SESSION,
    CMD_M4U_CLOSE_SESSION,
    
    CMD_M4U_CFG_PORT_ARRAY,

    CMD_M4U_SYSTRACE_MAP,  
    CMD_M4U_SYSTRACE_UNMAP,    
    CMD_M4U_SYSTRACE_TRANSACT,    
    
    CMD_M4U_LARB_BACKUP,    
    CMD_M4U_LARB_RESTORE,
    CMD_M4U_UNMAP_NONSEC_BUFFER,    
    
}m4u_cmd_t;


#define M4U_RET_OK              0    
#define M4U_RET_UNKNOWN_CMD     -1  
#define M4U_RET_NO_PERM         -2


#define EXIT_ERROR                  ((uint32_t)(-1))

typedef struct {
    int a;
    int b;
    int result;
}m4u_add_param_t;

#define M4U_SIN_NAME_LEN 12

typedef struct {
    int sid;
    char name[M4U_SIN_NAME_LEN];
}m4u_session_param_t;

typedef struct {
    int port;
    int virt;
    int sec;
    int distance;
    int direction;    
}m4u_cfg_port_param_t;

typedef struct {
    int port;
    unsigned int mva;
    unsigned int size;
    unsigned int pa;
}m4u_buf_param_t;

typedef struct {
    unsigned int nonsec_pt_pa;
    int l2_en;
    unsigned int sec_pt_pa;
    unsigned int sec_pa_start;
    unsigned int sec_pa_size;
}m4u_init_param_t;

typedef struct {
	
	unsigned long pa;
	unsigned long size;
}m4u_systrace_param_t;

typedef struct {
	unsigned char m4u_port_array[(M4U_PORT_NR+1)/2];
}m4u_cfg_port_array_param_t;

typedef struct {
	unsigned int larb_idx;
}m4u_larb_restore_param_t;


typedef struct {
      unsigned int     cmd;
      unsigned int     retval_for_tbase; //it must be 0
      unsigned int     rsp;

    union {
        m4u_session_param_t session_param;
        m4u_cfg_port_param_t port_param;
        m4u_buf_param_t buf_param;
        m4u_init_param_t init_param;
		m4u_cfg_port_array_param_t port_array_param;
#ifdef __M4U_SECURE_SYSTRACE_ENABLE__		
		m4u_systrace_param_t systrace_param;
#endif
		m4u_larb_restore_param_t larb_param;
    };

} m4u_msg_t;


#if defined(__cplusplus)
}
#endif


#endif // TLFOO_H_
