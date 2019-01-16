/* to avoid disclosing any secret and let customer know we have hacc hardware, 
   the file name 'hacc' is changed to 'hacc_hw' in kernel driver */

#ifndef HACC_MACH_H
#define HACC_MACH_H

#include "sec_osal_light.h"

/******************************************************************************
 * CHIP SELECTION
 ******************************************************************************/
#include <mach/mt_typedefs.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>

/******************************************************************************
 * MACROS DEFINITIONS                                                         
 ******************************************************************************/
#define AES_BLK_SZ_ALIGN(size)      ((size) & ~((AES_BLK_SZ << 3) - 1))


/******************************************************************************
 * HARDWARE DEFINITIONS                                                         
 ******************************************************************************/
#define HACC_CG                      (0x1 << 10)

#define HACC_AES_TEST_SRC            (0x02000000)
#define HACC_AES_TEST_TMP            (0x02100000)
#define HACC_AES_TEST_DST            (0x02200000)

#define HACC_CFG_0                    (0x5a5a3257)    /* CHECKME */
#define HACC_CFG_1                    (0x66975412)    /* CHECKME */
#define HACC_CFG_2                    (0x66975412)    /* CHECKME */
#define HACC_CFG_3                    (0x5a5a3257)    /* CHECKME */

#define HACC_CON                     (HACC_BASE+0x0000)
#define HACC_ACON                    (HACC_BASE+0x0004)
#define HACC_ACON2                   (HACC_BASE+0x0008)
#define HACC_ACONK                   (HACC_BASE+0x000C)
#define HACC_ASRC0                   (HACC_BASE+0x0010)
#define HACC_ASRC1                   (HACC_BASE+0x0014)
#define HACC_ASRC2                   (HACC_BASE+0x0018)
#define HACC_ASRC3                   (HACC_BASE+0x001C)
#define HACC_AKEY0                   (HACC_BASE+0x0020)
#define HACC_AKEY1                   (HACC_BASE+0x0024)
#define HACC_AKEY2                   (HACC_BASE+0x0028)
#define HACC_AKEY3                   (HACC_BASE+0x002C)
#define HACC_AKEY4                   (HACC_BASE+0x0030)
#define HACC_AKEY5                   (HACC_BASE+0x0034)
#define HACC_AKEY6                   (HACC_BASE+0x0038)
#define HACC_AKEY7                   (HACC_BASE+0x003C)
#define HACC_ACFG0                   (HACC_BASE+0x0040)
#define HACC_AOUT0                   (HACC_BASE+0x0050)
#define HACC_AOUT1                   (HACC_BASE+0x0054)
#define HACC_AOUT2                   (HACC_BASE+0x0058)
#define HACC_AOUT3                   (HACC_BASE+0x005C)
#define HACC_SW_OTP0                 (HACC_BASE+0x0060)
#define HACC_SW_OTP1                 (HACC_BASE+0x0064)
#define HACC_SW_OTP2                 (HACC_BASE+0x0068)
#define HACC_SW_OTP3                 (HACC_BASE+0x006c)
#define HACC_SW_OTP4                 (HACC_BASE+0x0070)
#define HACC_SW_OTP5                 (HACC_BASE+0x0074)
#define HACC_SW_OTP6                 (HACC_BASE+0x0078)
#define HACC_SW_OTP7                 (HACC_BASE+0x007c)
#define HACC_SECINIT0                (HACC_BASE+0x0080)
#define HACC_SECINIT1                (HACC_BASE+0x0084)
#define HACC_SECINIT2                (HACC_BASE+0x0088)
#define HACC_MKJ                     (HACC_BASE+0x00a0)

/* AES */
#define HACC_AES_DEC                 0x00000000
#define HACC_AES_ENC                 0x00000001
#define HACC_AES_MODE_MASK           0x00000002
#define HACC_AES_ECB                 0x00000000
#define HACC_AES_CBC                 0x00000002
#define HACC_AES_TYPE_MASK           0x00000030
#define HACC_AES_128                 0x00000000
#define HACC_AES_192                 0x00000010
#define HACC_AES_256                 0x00000020
#define HACC_AES_CHG_BO_MASK         0x00001000
#define HACC_AES_CHG_BO_OFF          0x00000000
#define HACC_AES_CHG_BO_ON           0x00001000
#define HACC_AES_START               0x00000001
#define HACC_AES_CLR                 0x00000002
#define HACC_AES_RDY                 0x00008000

/* AES key relevant */
#define HACC_AES_BK2C                0x00000010
#define HACC_AES_R2K                 0x00000100

/* SECINIT magic */
#define HACC_SECINIT0_MAGIC          0xAE0ACBEA
#define HACC_SECINIT1_MAGIC          0xCD957018
#define HACC_SECINIT2_MAGIC          0x46293911


/******************************************************************************
 * CONSTANT DEFINITIONS                                                       
 ******************************************************************************/
#define HACC_AES_MAX_KEY_SZ          (32)
#define AES_CFG_SZ                   (16)
#define AES_BLK_SZ                  (16)
#define HACC_HW_KEY_SZ               (16)
#define _CRYPTO_SEED_LEN            (16)

/* In order to support NAND writer and keep MTK secret,
   use MTK HACC seed and custom crypto seed to generate SW key
   to encrypt SEC_CFG */
#define MTK_HACC_SEED                (0x1)

/******************************************************************************
 * TYPE DEFINITIONS                                                           
 ******************************************************************************/
typedef enum
{   
    AES_ECB_MODE,
    AES_CBC_MODE
} AES_MODE;

typedef enum
{   
    AES_DEC,
    AES_ENC
} AES_OPS;

typedef enum
{
    AES_KEY_128 = 16,
    AES_KEY_192 = 24,
    AES_KEY_256 = 32
} AES_KEY;

typedef enum
{   
    AES_SW_KEY,
    AES_HW_KEY,
    AES_HW_WRAP_KEY
} AES_KEY_ID;

typedef struct {
    U8 config[AES_CFG_SZ];
} AES_CFG;

typedef struct {
    U32 size;
    U8  seed[HACC_AES_MAX_KEY_SZ];
} AES_KEY_SEED;

struct hacc_context {
    AES_CFG cfg;
    U32    blk_sz;
    U8     sw_key[HACC_AES_MAX_KEY_SZ];
    U8     hw_key[HACC_AES_MAX_KEY_SZ];
};

#endif
