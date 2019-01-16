/**
 * Copyright (C) 2009 Koninklijke Philips Electronics N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Koninklijke Philips Electronics N.V. and is confidential in
 * nature. Under no circumstances is this software to be  exposed to or placed
 * under an Open Source License of any type without the expressed written
 * permission of Koninklijke Philips Electronics N.V.
 *
 * \file          tmbslTDA9989_InOut_l.h
 *
 * \version       $Revision: 2 $
 *
 *
*/

#ifndef TMBSLTDA9989_INOUT_L_H
#define TMBSLTDA9989_INOUT_L_H

/*============================================================================*/
/*                       INCLUDE FILES                                        */
/*============================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/*                       MACRO DEFINITIONS                                    */
/*============================================================================*/

#define IS_TV(fmt) (fmt >= HDMITX_VFMT_TV_MIN && fmt <= HDMITX_VFMT_TV_MAX)
#define IS_VALID_FMT(fmt) IS_TV(fmt)
#ifdef FORMAT_PC
#define IS_PC(fmt) (fmt >= HDMITX_VFMT_PC_MIN && fmt <= HDMITX_VFMT_PC_MAX)
#define IS_VALID_FMT(fmt) (IS_TV(fmt) || IS_PC(fmt))
#endif
#define VIC2REG_LOOP(array, idx) do {                           \
   hash = (struct vic2reg *)(array);                         \
   for (i = 0; i < (sizeof(array)/sizeof(struct vic2reg)); i++) { \
      if (hash[i].vic == fmt) {                              \
         (*idx) = hash[i].reg;                               \
	 break;                                            \
      }                                                    \
   }                                                       \
} while (0);

/*============================================================================*/
/*                       ENUM OR TYPE DEFINITIONS                             */
/*============================================================================*/

	typedef struct {
		UInt16 Register;
		UInt8 MaskSwap;
		UInt8 MaskMirror;
	} tmbslTDA9989RegVip;

/*============================================================================*/
/*                       EXTERN DATA DEFINITION                               */
/*============================================================================*/

	extern tmHdmiTxRegMaskVal_t kCommonPllCfg[];

/**
 * Table of PLL settings registers to configure for 480i and 576i vinFmt
 */
	extern tmHdmiTxRegMaskVal_t kVfmt480i576iPllCfg[];

/**
 * Table of PLL settings registers to configure for single mode pixel rate,
 * vinFmt 480i or 576i only
 */
	extern tmHdmiTxRegMaskVal_t kSinglePrateVfmt480i576iPllCfg[];

/**
 * Table of PLL settings registers to configure for single repeated mode pixel rate,
 * vinFmt 480i or 576i only
 */
	extern tmHdmiTxRegMaskVal_t kSrepeatedPrateVfmt480i576iPllCfg[];

/**
 * Table of PLL settings registers to configure for other vinFmt than 480i and 576i
 */
	extern tmHdmiTxRegMaskVal_t kVfmtOtherPllCfg[];

/**
 * Table of PLL settings registers to configure single mode pixel rate,
 * vinFmt other than 480i or 576i
 */
	extern tmHdmiTxRegMaskVal_t kSinglePrateVfmtOtherPllCfg[];

/**
 * Table of PLL settings registers to configure double mode pixel rate,
 * vinFmt other than 480i or 576i
 */
	extern tmHdmiTxRegMaskVal_t kDoublePrateVfmtOtherPllCfg[];


/*============================================================================*/
/*                       EXTERN FUNCTION PROTOTYPES                           */
/*============================================================================*/
	extern tmbslHdmiTxVidFmt_t calculateVidFmtIndex(tmbslHdmiTxVidFmt_t vidFmt);


#ifdef __cplusplus
}
#endif
#endif				/* TMBSLTDA9989_INOUT_L_H */
/*============================================================================*//*                            END OF FILE                                     *//*============================================================================*/
