#ifndef AC_REGION_H
#define AC_REGION_H

#include "sec_osal_light.h"

/**************************************************************************
 * [FILE SYSTEM SECROIMG PATH]
 **************************************************************************/
/* it is only used for non-security platform */
#define FS_SECRO_PATH                   "/system/secro/AC_REGION"


/**************************************************************************
 * [AC REGION ID]
 **************************************************************************/
#define ROM_SEC_AC_REGION_ID            "AND_AC_REGION"
#define ROM_SEC_AC_REGION_ID_LEN        (13)

#define RES_FOR_HEADER                  (0x400) // 1KB
#define ROM_SEC_AC_SEARCH_LEN           0x100000 // 1MB

#define MAX_SECRO_V3_OFFSET             0xFFFFFFFF



/**************************************************************************
 * [AC-REGION HEADER FORNAT]
 **************************************************************************/
#define AC_H_MAGIC                      (0x48484848)
 
typedef struct {

    unsigned char                       m_id[16];
    unsigned int                        magic_number;

    unsigned int                        region_length;  /* include andro and sv5*/
    unsigned int                        region_offset; 

    unsigned int                        hash_length;  
    unsigned int                        hash_offset;
    
    unsigned int                        andro_length;   
    unsigned int                        andro_offset;   
    
    unsigned int                        md_length;   
    unsigned int                        md_offset;
    
    unsigned int                        md2_length;
    unsigned int                        md2_offset;     
    
    unsigned char                       world_phone_support;    /* for secro v5 format to support world phone case */
    unsigned char                       world_phone_md_count;   /* for secro v5 format to support world phone case */
    
    unsigned char                       reserve[2];
    
} AND_AC_HEADER_T;


/**************************************************************************
 * [AC-REGION ANDRO FORNAT]
 **************************************************************************/
#define AC_ANDRO_MAGIC                  (0x41414141)
#define AP_SECRO_MAX_LEN                (2939)

/* control flag */
#define FACTORY_EN_CODE                 (0x45)      

typedef struct {

    unsigned int                        magic_number;       
    unsigned char                       sml_aes_key[32]; /* sml aes key */ 
    unsigned char                       factory_en;    
    unsigned char                       reserve2[AP_SECRO_MAX_LEN];
    
} AND_AC_ANDRO_T;

/**************************************************************************
 * [AC-REGION MD FORNAT]
 **************************************************************************/
#define AC_MD_MAGIC                     (0x35353535)
#define MD_SECRO_MAX_LEN                (4092)

typedef struct {

    unsigned int                        magic_number;
    unsigned char                       reserve[MD_SECRO_MAX_LEN];
    
} AND_AC_MD_T;

#define AC_MD2_MAGIC                (0x36363636)
#define MD2_SECRO_MAX_LEN               (4092)

typedef struct {

    unsigned int                        magic_number;
    unsigned char                       reserve[MD2_SECRO_MAX_LEN];
    
} AND_AC_MD2_T;


/**************************************************************************
 * [Padding format to be able to align to 512/1024/2048/4096 for flash]
 **************************************************************************/
typedef struct {      
    unsigned int                        md_num;
    unsigned int                        md_len;
    unsigned char                       md_name[64];
    unsigned char                       reserve[8];
} AND_AC_MD_INFO_V3a_T;

#define MAX_V5_SUPPORT_MD_NUM   10

typedef struct {
    AND_AC_MD_INFO_V3a_T md_v3a_info[MAX_V5_SUPPORT_MD_NUM];
    unsigned char reserve[224];
} AND_SECROIMG_PADDING_T;



/**************************************************************************
 * [AC-REGION FORNAT]
 **************************************************************************/
typedef struct {
    
    AND_AC_HEADER_T                     m_header;   /* 64 */
    AND_AC_ANDRO_T                      m_andro;    /* 0xBA0  : 2976 */
    AND_AC_MD_T                         m_md;       /* 0x1000 : 4096 */ 
    AND_AC_MD2_T                        m_md2;      /* 0x1000 : 4096 */      
    unsigned char                       hash[32];   /* it can be extended to SHA256 */
    AND_SECROIMG_PADDING_T              m_padding;      /* 0x400  : 1024 ,use padding area to store MD1 ~ MD10 modem_info*/
} AND_SECROIMG_T;


/**************************************************************************
 * [SECROIMG v5a FORNAT for v5] - To support world phone
 **************************************************************************/
typedef struct {
    unsigned char                       m_identifier[16];
    unsigned int                        magic_number;

    unsigned int                        region_len;  /* include andro, sv5 and hash */
    unsigned int                        region_off; 

    unsigned int                        hash_len;    
    unsigned int                        hash_off;
    
    unsigned int                        md_info_len;
    unsigned int                        md_info_off;   
    
    unsigned int                        md_len;
    unsigned int                        md_off; 
    
    unsigned char                       reserve[12];    
} AND_AC_HEADER_V5a_T;

#define AC_MD_INFO_MAGIC                (0x42424242)

typedef struct {
    unsigned int                        magic_number;       /* Should be 0x42424242 */  
    unsigned char                       md_name[64];
    unsigned int                        md_num;
    unsigned char                       reserve[2904];    
} AND_AC_MD_INFO_V5a_T;

#define AC_SV5_MAGIC_MD_V5a             (0x37373737)
#define MD_V5a_SECRO_MAX_LEN            (8188)

typedef struct {
    unsigned int                        magic_number;       /* Should be 0x37373737 */
    unsigned char                       reserve[MD_V5a_SECRO_MAX_LEN];    
} AND_AC_MD_V5a_T;

#define AND_SECROIMG_V5a_SIZE              (0x3000)

typedef struct {    
    AND_AC_HEADER_V5a_T                     m_header_v5a;   /* 64B */
    AND_AC_MD_INFO_V5a_T                    m_md_info_v5a;  /* 0xBA0  : 2976B */
    AND_AC_MD_V5a_T                         m_md_sro_v5a;   /* 0x1000 : 8192B */ 
    unsigned char                           hash_v5a[32];   /* 32B, it can be extended to SHA256 */   
    AND_SECROIMG_PADDING_T                  padding;
} AND_SECROIMG_V5a_T;
    

//typedef enum{
//    SECRO_MD1 = 0,
//    SECRO_MD2,    
//} SECRO_USER;

/**************************************************************************
 * [EXPORT FUNCTION]
 **************************************************************************/
extern unsigned int sec_secro_check (void);
extern bool sec_secro_ac (void);
extern uint32 sec_secro_v5_check (void);
extern unsigned char masp_secro_en (void);


#endif /* AC_REGION_H */
