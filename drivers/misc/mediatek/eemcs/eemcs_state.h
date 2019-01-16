#ifndef __EEMCS_STATE_H__
#define __EEMCS_STATE_H__

#include "eemcs_kal.h"

/*
 *  For user program to query the EEMCS detail state by
 *  CCCI_IOC_CHECK_STATE  _IOR(CCCI_IOC_MAGIC, 12, unsigned int)
 */
typedef enum EEMCS_STATE_E {
    EEMCS_INVALID =0,
    EEMCS_GATE,
    EEMCS_INIT,
    EEMCS_XBOOT,
    EEMCS_MOLY_HS_P1,
    EEMCS_MOLY_HS_P2,
    EEMCS_BOOTING_DONE,
    EEMCS_EXCEPTION,
    EEMCS_STATE_MAX,
}EEMCS_STATE;

/* 
 * For user program to query the MD state by
 * CCCI_IOC_GET_MD_STATE  _IOR(CCCI_IOC_MAGIC, 1, unsigned int)
 */
typedef enum MD_STATE_E{
	MD_STATE_INVALID = 0,  /* MD_BOOT_STAGE_0 */
	MD_STATE_INIT    = 1,  /* MD_BOOT_STAGE_1 */
	MD_STATE_READY   = 2,  /* MD_BOOT_STAGE_2 */
	MD_STATE_EXPT    = 3   /* MD_BOOT_STAGE_EXCEPTION */
}MD_STATE;

typedef struct{
    struct list_head list;
    void (*callback)(EEMCS_STATE data);
} EEMCS_STATE_CALLBACK_T;

void eemcs_state_callback_init(void);
kal_bool eemcs_state_callback(EEMCS_STATE state);
kal_bool eemcs_state_callback_register(EEMCS_STATE_CALLBACK_T *node);
kal_bool eemcs_state_callback_unregister(EEMCS_STATE_CALLBACK_T *node);

KAL_UINT32 check_device_state(void);
kal_bool eemcs_device_ready(void);
kal_bool change_device_state(KAL_UINT32 state);
bool eemcs_on_reset(void);
#endif // __EEMCS_STATE_H__
