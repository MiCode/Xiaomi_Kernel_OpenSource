/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/* to avoid disclosing any secret and let customer know we have hacc hardware,
   the file name 'hacc' is changed to 'hacc_hw' in kernel driver */

#ifndef HACC_MACH_H
#define HACC_MACH_H

#include "sec_osal_light.h"

/******************************************************************************
 * CHIP SELECTION
 ******************************************************************************/
/*
#include <mach/mt_typedefs.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>
*/
#ifdef CONFIG_ARM64
extern unsigned long long hacc_base;
#else
extern unsigned int hacc_base;
#endif
#if 0
#ifdef CONFIG_ARM64
extern unsigned long long es_base;
#else
extern unsigned int es_base;
#endif
#endif
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

#define HACC_CFG_0                    (0x5a5a3257)	/* CHECKME */
#define HACC_CFG_1                    (0x66975412)	/* CHECKME */
#define HACC_CFG_2                    (0x66975412)	/* CHECKME */
#define HACC_CFG_3                    (0x5a5a3257)	/* CHECKME */

#define HACC_CON                     (hacc_base+0x0000)
#define HACC_ACON                    (hacc_base+0x0004)
#define HACC_ACON2                   (hacc_base+0x0008)
#define HACC_ACONK                   (hacc_base+0x000C)
#define HACC_ASRC0                   (hacc_base+0x0010)
#define HACC_ASRC1                   (hacc_base+0x0014)
#define HACC_ASRC2                   (hacc_base+0x0018)
#define HACC_ASRC3                   (hacc_base+0x001C)
#define HACC_AKEY0                   (hacc_base+0x0020)
#define HACC_AKEY1                   (hacc_base+0x0024)
#define HACC_AKEY2                   (hacc_base+0x0028)
#define HACC_AKEY3                   (hacc_base+0x002C)
#define HACC_AKEY4                   (hacc_base+0x0030)
#define HACC_AKEY5                   (hacc_base+0x0034)
#define HACC_AKEY6                   (hacc_base+0x0038)
#define HACC_AKEY7                   (hacc_base+0x003C)
#define HACC_ACFG0                   (hacc_base+0x0040)
#define HACC_AOUT0                   (hacc_base+0x0050)
#define HACC_AOUT1                   (hacc_base+0x0054)
#define HACC_AOUT2                   (hacc_base+0x0058)
#define HACC_AOUT3                   (hacc_base+0x005C)
#define HACC_SW_OTP0                 (hacc_base+0x0060)
#define HACC_SW_OTP1                 (hacc_base+0x0064)
#define HACC_SW_OTP2                 (hacc_base+0x0068)
#define HACC_SW_OTP3                 (hacc_base+0x006c)
#define HACC_SW_OTP4                 (hacc_base+0x0070)
#define HACC_SW_OTP5                 (hacc_base+0x0074)
#define HACC_SW_OTP6                 (hacc_base+0x0078)
#define HACC_SW_OTP7                 (hacc_base+0x007c)
#define HACC_SECINIT0                (hacc_base+0x0080)
#define HACC_SECINIT1                (hacc_base+0x0084)
#define HACC_SECINIT2                (hacc_base+0x0088)
#define HACC_MKJ                     (hacc_base+0x00a0)

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
typedef enum {
	AES_ECB_MODE,
	AES_CBC_MODE
} AES_MODE;

typedef enum {
	AES_DEC,
	AES_ENC
} AES_OPS;

typedef enum {
	AES_KEY_128 = 16,
	AES_KEY_192 = 24,
	AES_KEY_256 = 32
} AES_KEY;

typedef enum {
	AES_SW_KEY,
	AES_HW_KEY,
	AES_HW_WRAP_KEY
} AES_KEY_ID;

typedef struct {
	unsigned char config[AES_CFG_SZ];
} AES_CFG;

typedef struct {
	unsigned int size;
	unsigned char seed[HACC_AES_MAX_KEY_SZ];
} AES_KEY_SEED;

struct hacc_context {
	AES_CFG cfg;
	unsigned int blk_sz;
	unsigned char sw_key[HACC_AES_MAX_KEY_SZ];
	unsigned char hw_key[HACC_AES_MAX_KEY_SZ];
};

/* --------------------------------------------------------------------------- */
/* Register Manipulations */
/* --------------------------------------------------------------------------- */

#define READ_REGISTER_UINT32(reg) \
	(*(volatile unsigned int * const)(reg))

#define WRITE_REGISTER_UINT32(reg, val) \
	((*(volatile unsigned int * const)(reg)) = (val))

#define INREG32(x)          READ_REGISTER_UINT32((unsigned int *)((void *)(x)))
#define OUTREG32(x, y)      WRITE_REGISTER_UINT32((unsigned int *)((void *)(x)), (unsigned int)(y))
#define SETREG32(x, y)      OUTREG32(x, INREG32(x)|(y))
#define CLRREG32(x, y)      OUTREG32(x, INREG32(x)&~(y))
#define MASKREG32(x, y, z)  OUTREG32(x, (INREG32(x)&~(y))|(z))

#define DRV_Reg32(addr)             INREG32(addr)
#define DRV_WriteReg32(addr, data)  OUTREG32(addr, data)
#define DRV_SetReg32(addr, data)    SETREG32(addr, data)
#define DRV_ClrReg32(addr, data)    CLRREG32(addr, data)


/******************************************************************************
 *  EXPORT FUNCTION
 ******************************************************************************/
extern unsigned int hacc_set_key(AES_KEY_ID id, AES_KEY key);
extern unsigned int hacc_do_aes(AES_OPS ops, unsigned char *src, unsigned char *dst,
				unsigned int size);
extern unsigned int hacc_init(AES_KEY_SEED *keyseed);
extern unsigned int hacc_deinit(void);
extern void HACC_V3_Init(bool encode, const unsigned int g_AC_CFG[]);
extern void HACC_V3_Run(volatile unsigned int *p_src, unsigned int src_len,
			volatile unsigned int *p_dst);
extern void HACC_V3_Terminate(void);

/******************************************************************************
 * EXTERNAL VARIABLE
 ******************************************************************************/
extern bool bHACC_HWWrapKeyInit;
extern bool bHACC_SWKeyInit;

#endif
