#ifndef SEC_CFG_H
#define SEC_CFG_H

/* EXPORT TO USERS */
#include "sec_cfg_v1.h"
#include "sec_cfg_v3.h"
#include "sec_cfg_common.h"
#include "sec_cfg_crypto.h"

/**************************************************************************
 *  SECCFG VERSION
 **************************************************************************/
typedef union
{
    SECURE_CFG_V1       v1;
    SECURE_CFG_V3       v3;
    
} SECCFG_U;

typedef union
{
    SECURE_IMG_INFO_V1  v1;
    SECURE_IMG_INFO_V3  v3;
    
} SEC_IMG_U;


/**************************************************************************
 *  EXPORT FUNCTIONS
 **************************************************************************/

#endif
