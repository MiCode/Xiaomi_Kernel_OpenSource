#ifndef SEC_CFG_V1_H
#define SEC_CFG_V1_H

/**************************************************************************
 *  INCLUDE SEC CFG COMMON
 **************************************************************************/
#include "sec_cfg_common.h"

/**************************************************************************
 *  INCLUDE SIGN HEADER FORMAT
 **************************************************************************/
#include "sec_sign_header.h"

/**************************************************************************
 *  SECURE DOWNLOAD LOCK TABLE
 **************************************************************************/
#define MAX_IMG_LOCK_COUNT      (20)
#define IMAGE_LOCK_MAGIC        (0x4C4C4C4C) /* LLLL */

typedef struct
{
    unsigned int                magic_number; 
    unsigned char               name[32];   /* partition name */
    unsigned char               unlocked;

} IMAGE_DL_LOCK_INFO;

typedef struct
{
    unsigned int                magic_number;    
    unsigned char               lock_not_all;    
    IMAGE_DL_LOCK_INFO          lock_info[MAX_IMG_LOCK_COUNT];
   
} SECURE_DL_LOCK_TABLE;

/**************************************************************************
 *  SECURE IMAGE HEADER
 **************************************************************************/

typedef struct
{   
    unsigned int                magic_number;     
    unsigned char               name[16];     /* index for identification */    
    unsigned int                real_offset;  /* download agent will update the real offset */        
    ROM_TYPE                    image_type;   /* yaffs2 format or raw binary */
    SEC_IMG_ATTR                attr;         /* image attributes */
    SEC_IMG_HEADER_U            header;
    unsigned char               signature_hash [HASH_SIG_LEN];
    
} SECURE_IMG_INFO_V1;

/**************************************************************************
 *  SECURE CFG FORMAT
 **************************************************************************/
#define SECURE_IMAGE_COUNT      (12)
#define SEC_CFG_RESERVED        (4)

/* ================================= */
/* SECCFG FORMAT                     */
/* ================================= */
typedef struct
{   
    unsigned char               id[16];   
    unsigned int                magic_number;
    unsigned int                lib_ver;     

    unsigned int                sec_cfg_size; 

    unsigned char               sw_sec_lock_try;    
    unsigned char               sw_sec_lock_done;
    
    unsigned short              page_size; 
    unsigned int                page_count;

    /* ================== */
    /* encrypted region { */
    /* ================== */  
    SECURE_IMG_INFO_V1          image_info [SECURE_IMAGE_COUNT];
    SIU_STATUS                  siu_status; 
    unsigned char               reserve [SEC_CFG_RESERVED]; 
    SECCFG_STATUS               status;
    SECCFG_ATTR                 attr;
    /* ================== */
    /* encrypted region } */
    /* ================== */  
    
    SECURE_DL_LOCK_TABLE        lock_table;  
    unsigned int                end_pattern;

} SECURE_CFG_V1;

#endif // SEC_CFG_V1_H