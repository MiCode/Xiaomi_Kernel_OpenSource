#ifndef SEC_CFG_V3_H
#define SEC_CFG_V3_H

/**************************************************************************
 *  INCLUDE SEC CFG COMMON
 **************************************************************************/
#include "sec_cfg_common.h"

/**************************************************************************
 *  INCLUDE SIGN HEADER FORMAT
 **************************************************************************/
#include "sec_sign_header.h"

/**************************************************************************
 *  SECCFG EXTENSION REGION
 **************************************************************************/
#define EXT_REGION_BUF_SIZE     (4096)        /* used to store extension headers */
 
typedef struct
{    
    unsigned int                content_len;  /* total used data length */    
    unsigned char               buf [EXT_REGION_BUF_SIZE];
    
} SECCFG_EXT_REGION;

/**************************************************************************
 *  SECURE IMAGE HEADER
 **************************************************************************/

typedef struct
{   
    unsigned int                magic_number;     
    unsigned char               name [16];    /* index for identification */    
    unsigned int                real_offset;  /* download agent will update the real offset */        
    ROM_TYPE                    image_type;   /* yaffs2 format or raw binary */
    SEC_IMG_ATTR                image_attr;   /* image attributes */
    SEC_IMG_HEADER_U            header;       /* image sign header */
    unsigned int                ext_offset;   /* offset of extension header */    
    unsigned int                ext_len;      /* length of extension header */        
    
} SECURE_IMG_INFO_V3;

/**************************************************************************
 *  SECURE CFG FORMAT
 **************************************************************************/
#define SECURE_IMAGE_COUNT_V3   (20)

/* ================================= */
/* SECCFG FORMAT                     */
/* ================================= */
typedef struct
{   
    unsigned char               id[16];   
    unsigned int                magic_number;
    
    unsigned int                seccfg_ver;     
    unsigned int                seccfg_size; 

    unsigned int                seccfg_enc_offset; 
    unsigned int                seccfg_enc_len; 
    
    unsigned char               sw_sec_lock_try;
    unsigned char               sw_sec_lock_done;
    
    unsigned short              page_size; 
    unsigned int                page_count;

    /* ================== */
    /* encrypted region { */
    /* ================== */    
    SECURE_IMG_INFO_V3          image_info [SECURE_IMAGE_COUNT_V3];
    SIU_STATUS                  siu_status; 
    SECCFG_STATUS               seccfg_status;
    SECCFG_ATTR                 seccfg_attr;     
    SECCFG_EXT_REGION           seccfg_ext;
    /* ================== */    
    /* encrypted region } */
    /* ================== */    
    
    unsigned int                end_pattern;

} SECURE_CFG_V3;

#endif // SEC_CFG_V3_H
