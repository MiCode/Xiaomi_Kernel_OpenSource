#ifndef __EEMCS_BOOT_TRACE_H__
#define __EEMCS_BOOT_TRACE_H__

#include <asm/atomic.h>
#include <linux/skbuff.h>
#include <linux/wait.h>

#include "eemcs_ccci.h"
#include "eemcs_kal.h"
#include "eemcs_boot.h"
#include "eemcs_state.h"

typedef enum EEMCS_BOOT_TRACE_TYPE_e {
    START_OF_TRA_TYPE,
    TRA_TYPE_BOOT_STATE = START_OF_TRA_TYPE,
    TRA_TYPE_EEMCS_STATE,
    TRA_TYPE_XCMD_RX,
    TRA_TYPE_XCMD_TX,
    TRA_TYPE_MBX_RX,
    TRA_TYPE_MBX_TX,
    END_OF_TRA_TYPE,
    TRA_TYPE_MAX = END_OF_TRA_TYPE,
} EEMCS_BOOT_TRACE_TYPE;

typedef enum EEMCS_BOOT_TRACE_OUT_TYPE_e {
    OUT_TYPE_EEMCS_STATE,
    OUT_TYPE_BOOT_STATE,
    OUT_TYPE_XCMD,
    OUT_TYPE_MBX,
    OUT_TYPE_ALL,
    OUT_TYPE_COUNT,
} EEMCS_BOOT_TRACE_OUT_TYPE;

typedef struct EEMCS_BOOT_TRACE_SET_st {
    EEMCS_BOOT_TRACE_TYPE type;
    union {
        KAL_UINT32 state;
        KAL_UINT32 mbx_data[2];
        XBOOT_CMD xcmd;
        XBOOT_CMD_GETBIN xcmd_getbin;
    } trace;
} EEMCS_BOOT_TRACE_SET;

typedef struct EEMCS_BOOT_TRACE_st {
    struct sk_buff_head log_skb_list;
    KAL_UINT32 inited;
} EEMCS_BOOT_TRACE;

KAL_UINT32 eemcs_boot_trace_init(void);
KAL_UINT32 eemcs_boot_trace_deinit(void);
KAL_UINT32 eemcs_boot_trace_state(EEMCS_STATE state);
KAL_UINT32 eemcs_boot_trace_boot(EEMCS_BOOT_STATE state);
KAL_UINT32 eemcs_boot_trace_mbx(KAL_UINT32 *val, KAL_UINT32 size, EEMCS_BOOT_TRACE_TYPE type);
KAL_UINT32 eemcs_boot_trace_xcmd(XBOOT_CMD *xcmd, EEMCS_BOOT_TRACE_TYPE type);
KAL_UINT32 eemcs_boot_trace_xcmd_file(KAL_UINT32 offset, KAL_UINT32 len, KAL_UINT8 checksum);
void eemcs_boot_trace_out(EEMCS_BOOT_TRACE_OUT_TYPE type);

#endif // __EEMCS_BOOT_TRACE_H__
