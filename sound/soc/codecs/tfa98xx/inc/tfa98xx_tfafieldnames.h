/* 
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


typedef struct TfaBfName {
   unsigned short bfEnum;
   char  *bfName;
} tfaBfName_t;

typedef struct TfaIrqName {
	unsigned short irqEnum;
	char  *irqName;
} tfaIrqName_t;

#include "tfa1_tfafieldnames.h"
#include "tfa2_tfafieldnames_N1C.h"
/* diffs for specific devices */
#include "tfa9887_tfafieldnames.h"
#include "tfa9890_tfafieldnames.h"
#include "tfa9891_tfafieldnames.h"
#include "tfa9872_tfafieldnames.h"
#include "tfa9912_tfafieldnames.h"
#include "tfa9896_tfafieldnames.h"
#include "tfa9873_tfafieldnames.h"
#include "tfa9873_tfafieldnames_B0.h"
#include "tfa9874_tfafieldnames.h"
#include "tfa9878_tfafieldnames.h"
#include "tfa9894_tfafieldnames.h"
#include "tfa9894_tfafieldnames_N2.h"

/* missing 'common' defs break the build but unused in TFA1 context */
#define TFA1_BF_AMPINSEL -1
#define TFA1_BF_MANSCONF -1
#define TFA1_BF_MANCOLD  -1
#define TFA1_BF_INTSMUTE -1
#define TFA1_BF_CFSMR    -1
#define TFA1_BF_CFSML    -1
#define TFA1_BF_DCMCCAPI -1
#define TFA1_BF_DCMCCSB  -1
#define TFA1_BF_USERDEF  -1
#define TFA1_BF_MANSTATE -1
#define TFA1_BF_MANOPER  -1
#define TFA1_BF_REFCKSEL -1
#define TFA1_BF_VOLSEC	 -1
#define TFA1_BF_FRACTDEL -1
#define TFA1_BF_ACKDMG	 -1
#define TFA1_BF_SSRIGHTE -1
#define TFA1_BF_SSLEFTE	 -1
#define TFA1_BF_R25CL	 -1
#define TFA1_BF_R25CR	 -1
#define TFA1_BF_SWPROFIL 0x8045    /*!< profile save   */
#define TFA1_BF_SWVSTEP  0x80a5    /*!< vstep save  */

/* missing 'common' defs break the build */
#define TFA2_BF_CFSM -1


/* MTP access uses registers
 *  defs are derived from corresponding bitfield names as used in the BF macros
 */
#define MTPKEY2  	MTPK		/* unlock key2 MTPK */
#define MTP0     	MTPOTC 	/* MTP data */
#define MTP_CONTROL CIMTP	/* copy i2c to mtp */

/* interrupt enable register uses HW name in TFA2 */
#define TFA2_BF_INTENVDDS TFA2_BF_IEVDDS


/* TFA9891 specific bit field names */
#define TFA1_BF_SAAMGAIN 0x2202
#define TFA2_BF_SAAMGAIN -1

/* TFA9872 specific bit field names */
#define TFA2_BF_IELP0 TFA9872_BF_IELP0
#define TFA2_BF_ISTLP0 TFA9872_BF_ISTLP0
#define TFA2_BF_IPOLP0 TFA9872_BF_IPOLP0
#define TFA2_BF_IELP1 TFA9872_BF_IELP1
#define TFA2_BF_ISTLP1 TFA9872_BF_ISTLP1
#define TFA2_BF_IPOLP1 TFA9872_BF_IPOLP1
#define TFA2_BF_LP0 TFA9872_BF_LP0
#define TFA2_BF_LP1 TFA9872_BF_LP1
#define TFA2_BF_R25C TFA9872_BF_R25C
#define TFA2_BF_SAMMODE TFA9872_BF_SAMMODE

/* interrupt bit field names of TFA2 and TFA1 do not match */
#define TFA1_BF_IEACS TFA1_BF_INTENACS
#define TFA1_BF_IPOACS TFA1_BF_INTPOLACS
#define TFA1_BF_ISTACS TFA1_BF_INTOACS
#define TFA1_BF_ISTVDDS TFA1_BF_INTOVDDS
#define TFA1_BF_ICLVDDS TFA1_BF_INTIVDDS
#define TFA1_BF_IPOVDDS TFA1_BF_INTPOLVDDS
#define TFA1_BF_IENOCLK TFA1_BF_INTENNOCLK
#define TFA1_BF_ISTNOCLK TFA1_BF_INTONOCLK
#define TFA1_BF_IPONOCLK TFA1_BF_INTPOLNOCLK

/* interrupt bit fields not available on TFA1 */
#define TFA1_BF_IECLKOOR -1
#define TFA1_BF_ISTCLKOOR -1
#define TFA1_BF_IEMWSRC -1
#define TFA1_BF_ISTMWSRC -1
#define TFA1_BF_IPOMWSRC -1
#define TFA1_BF_IEMWSMU -1
#define TFA1_BF_ISTMWSMU -1
#define TFA1_BF_IPOMWSMU -1
#define TFA1_BF_IEMWCFC -1
#define TFA1_BF_ISTMWCFC -1
#define TFA1_BF_IPOMWCFC -1
#define TFA1_BF_CLKOOR -1
#define TFA1_BF_MANWAIT1 -1
#define TFA1_BF_MANWAIT2 -1
#define TFA1_BF_MANMUTE -1
#define TFA1_BF_IPCLKOOR -1
#define TFA1_BF_ICLCLKOOR -1
#define TFA1_BF_IPOSWS -1
#define TFA1_BF_IESWS -1
#define TFA1_BF_ISTSWS -1
#define TFA1_BF_IESPKS -1
#define TFA1_BF_ISTSPKS -1
#define TFA1_BF_IPOSPKS -1
#define TFA1_BF_IECLKS -1
#define TFA1_BF_ISTCLKS -1
#define TFA1_BF_IPOCLKS -1
#define TFA1_BF_IEAMPS -1
#define TFA1_BF_ISTAMPS -1
#define TFA1_BF_IPOAMPS -1
#define TFA1_BF_IELP0 -1
#define TFA1_BF_ISTLP0 -1
#define TFA1_BF_IPOLP0 -1
#define TFA1_BF_IELP1 -1
#define TFA1_BF_ISTLP1 -1
#define TFA1_BF_IPOLP1 -1
#define TFA1_BF_LP0 -1
#define TFA1_BF_LP1 -1
#define TFA1_BF_R25C -1
#define TFA1_BF_SAMMODE	 -1

/* TDM STATUS fields not available on TFA1 */
#define TFA1_BF_TDMLUTER -1
#define TFA1_BF_TDMERR -1
