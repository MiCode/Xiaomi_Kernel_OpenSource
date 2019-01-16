#ifndef _SBCHK_BASE_H
#define _SBCHK_BASE_H

#ifndef FALSE
  #define FALSE (0)
#endif

#ifndef TRUE
  #define TRUE  (1)
#endif

/**************************************************************************
 *  HASH CONFIGURATION
 **************************************************************************/
#define HASH_OUTPUT_LEN                     (20)

/**************************************************************************
 *  SBCHK PATH
 **************************************************************************/
#define SBCHK_ENGINE_PATH                   "/system/bin/sbchk"
#define SBCHK_MODULE_PATH                   "/system/lib/modules/sec.ko"
#define MODEM_CORE_MODULE_PATH              "/system/lib/modules/ccci.ko"
#define MODEM_PLAT_MODULE_PATH              "/system/lib/modules/ccci_plat.ko"
#define INIT_RC_PATH                        "init.rc"

/**************************************************************************
 *  SBCHK CHECK
 **************************************************************************/
#define SBCHK_BASE_HASH_CHECK               FALSE

/**************************************************************************
 *  SBCHK HASH VALUE
 **************************************************************************/
#if SBCHK_BASE_HASH_CHECK

    #error MUST fill the hash value of '/system/bin/sbchk', 
    'init.rc', '/system/lib/modules/sec.ko', '/system/lib/modules/ccci.ko' and 
    '/system/lib/modules/ccci_plat.ko'

    /* 
       Kernel will compare hash values to check if 
       the loaded program are exact as what you expect
       To ensure the hash value you fill is right. 
       The steps to obtain hash value are listed below;
       
           (1) Turn off SBCHK_BASE_HASH_CHECK   
           (2) Download images and record kernel log      
           (3) Find the string pattern '[SBCHK_BASE]' in kernel log      
           (4) The hash value of '/system/bin/sbchk' is calculated          
               [SBCHK_BASE] Calculate the hash value of '/system/bin/sbchk' =
               xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx              
           (5) Fill hash value in SBCHK_ENGINE_HASH   
           (6) The hash value of '/system/lib/modules/sec.ko' is calculated          
               [SBCHK_BASE] Calculate the hash value of '/system/lib/modules/sec.ko' =
               xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx              
           (7) Fill hash value in SBCHK_MODULE_HASH
           (8) The hash value of '/system/lib/modules/ccci.ko' is calculated          
               [SBCHK_BASE] Calculate the hash value of '/system/lib/modules/ccci.ko' =
               xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx              
           (9) Fill hash value in MODEM_CORE_MODULE_HASH           
           (10) The hash value of '/system/lib/modules/ccci_plat.ko' is calculated          
               [SBCHK_BASE] Calculate the hash value of '/system/lib/modules/ccci_plat.ko' =
               xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx              
           (11) Fill hash value in MODEM_PLAT_MODULE_HASH                      
           (12) The hash value of 'init.rc' is calculated          
               [SBCHK_BASE] Calculate the hash value of 'init.rc' =
               xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx              
           (13) Fill hash value in INIT_RC_HASH  

       PLEASE NOTE THAT !!!!!!
       The hash value may be changed if compile tool chain is updated or customer modify this user space program     
       Before delivering boot image, please double check if the hash value should be updated or not.
    */

    //#define SBCHK_ENGINE_HASH "d20086bd0626defe136e0a39f8790fbbe3ac00f2"
    //#define SBCHK_MODULE_HASH "475955d07809c5e78a6fa21a2718542ff7d660df"
    #define SBCHK_ENGINE_HASH ?????????????????????????????????????????
    #define SBCHK_MODULE_HASH ?????????????????????????????????????????
    #define MODEM_PLAT_MODULE_HASH ?????????????????????????????????????????    
    #define MODEM_CORE_MODULE_HASH ?????????????????????????????????????????    
    #define INIT_RC_HASH ?????????????????????????????????????????        

#else

    /* dummy */
    //#define SBCHK_ENGINE_HASH "3a816d2e275818cb12b839a10e838a1e10d729f7"
    #define SBCHK_ENGINE_HASH "0000000000000000000000000000000000000000"
    #define SBCHK_MODULE_HASH "0000000000000000000000000000000000000000"
    #define MODEM_PLAT_MODULE_HASH "0000000000000000000000000000000000000000"    
    #define MODEM_CORE_MODULE_HASH "0000000000000000000000000000000000000000"    
    #define INIT_RC_HASH "0000000000000000000000000000000000000000"        

#endif  

/**************************************************************************
 *  ERROR CODE
 **************************************************************************/
#define SEC_OK                              (0x0000)
#define SBCHK_BASE_OPEN_FAIL                (0x1000)
#define SBCHK_BASE_READ_FAIL                (0x1001)
#define SBCHK_BASE_HASH_INIT_FAIL           (0x1002)
#define SBCHK_BASE_HASH_DATA_FAIL           (0x1003)
#define SBCHK_BASE_HASH_CHECK_FAIL          (0x1004)
#define SBCHK_BASE_INDEX_OUT_OF_RANGE       (0xFFFFFFFF)

/**************************************************************************
 *  EXTERNAL FUNCTION
 **************************************************************************/
extern void sbchk_base(void);


/******************************************************************************
 * GLOBAL DATA
 ******************************************************************************/


#endif   /*_SBCHK_BASE*/
