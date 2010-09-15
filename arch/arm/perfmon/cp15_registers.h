/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
cp15_registers.h

DESCRIPTION: define macros for reading and writing to the cp registers
for the ARMv7

REV/DATE:  Fri Mar 18 15:54:32 EST 2005
*/

#ifndef __cp15_registers__
#define __cp15_registers__

#include "mcrmrc.h"

#define WCP15_SDER(reg)			MCR15(reg, 0, c1, c1, 1)
/*
* Performance Monitor Registers
*/
#define WCP15_PMACTLR(reg)		MCR15(reg, 0, c9, c15, 5)
#define WCP15_PMCCNTCR(reg)		MCR15(reg, 0, c9, c15, 2)
#define WCP15_PMCCNTR(reg)		MCR15(reg, 0, c9, c13, 0)
#define WCP15_PMCCNTSR(reg)		MCR15(reg, 0, c9, c13, 3)
#define WCP15_PMCNTENCLR(reg)		MCR15(reg, 0, c9, c12, 2)
#define WCP15_PMCNTENSET(reg)		MCR15(reg, 0, c9, c12, 1)
#define WCP15_PMCR(reg)			MCR15(reg, 0, c9, c12, 0)
#define WCP15_PMINTENCLR(reg)		MCR15(reg, 0, c9, c14, 2)
#define WCP15_PMINTENSET(reg)		MCR15(reg, 0, c9, c14, 1)
#define WCP15_PMOVSR(reg)		MCR15(reg, 0, c9, c12, 3)
#define WCP15_PMRLDR(reg)		MCR15(reg, 0, c9, c15, 4)
#define WCP15_PMSELR(reg)		MCR15(reg, 0, c9, c12, 5)
#define WCP15_PMSWINC(reg)		MCR15(reg, 0, c9, c12, 4)
#define WCP15_PMUSERENR(reg)		MCR15(reg, 0, c9, c14, 0)
#define WCP15_PMXEVCNTCR(reg)		MCR15(reg, 0, c9, c15, 0)
#define WCP15_PMXEVCNTR(reg)		MCR15(reg, 0, c9, c13, 2)
#define WCP15_PMXEVCNTSR(reg)		MCR15(reg, 0, c9, c15, 1)
#define WCP15_PMXEVTYPER(reg)		MCR15(reg, 0, c9, c13, 1)
#define WCP15_LPM0EVTYPER(reg)		MCR15(reg, 0, c15, c0, 0)
#define WCP15_LPM1EVTYPER(reg)		MCR15(reg, 1, c15, c0, 0)
#define WCP15_LPM2EVTYPER(reg)		MCR15(reg, 2, c15, c0, 0)
#define WCP15_LPM3EVTYPER(reg)		MCR15(reg, 3, c15, c0, 0)
#define WCP15_L2LPMEVTYPER(reg)		MCR15(reg, 3, c15, c2, 0)
#define WCP15_VLPMEVTYPER(reg)		MCR15(reg, 7, c11, c0, 0)
#define WCP15_L2VR3F1(reg)		MCR15(reg, 3, c15, c15, 1)

/*
* READ the registers
*/
#define RCP15_SDER(reg)			MRC15(reg, 0, c1, c1, 1)
/*
* Performance Monitor Registers
*/
#define RCP15_PMACTLR(reg)		MRC15(reg, 0, c9, c15, 5)
#define RCP15_PMCCNTCR(reg)		MRC15(reg, 0, c9, c15, 2)
#define RCP15_PMCCNTR(reg)		MRC15(reg, 0, c9, c13, 0)
#define RCP15_PMCCNTSR(reg)		MRC15(reg, 0, c9, c13, 3)
#define RCP15_PMCNTENCLR(reg)		MRC15(reg, 0, c9, c12, 2)
#define RCP15_PMCNTENSET(reg)		MRC15(reg, 0, c9, c12, 1)
#define RCP15_PMCR(reg)			MRC15(reg, 0, c9, c12, 0)
#define RCP15_PMINTENCLR(reg)		MRC15(reg, 0, c9, c14, 2)
#define RCP15_PMINTENSET(reg)		MRC15(reg, 0, c9, c14, 1)
#define RCP15_PMOVSR(reg)		MRC15(reg, 0, c9, c12, 3)
#define RCP15_PMRLDR(reg)		MRC15(reg, 0, c9, c15, 4)
#define RCP15_PMSELR(reg)		MRC15(reg, 0, c9, c12, 5)
#define RCP15_PMSWINC(reg)		MRC15(reg, 0, c9, c12, 4)
#define RCP15_PMUSERENR(reg)		MRC15(reg, 0, c9, c14, 0)
#define RCP15_PMXEVCNTCR(reg)		MRC15(reg, 0, c9, c15, 0)
#define RCP15_PMXEVCNTR(reg)		MRC15(reg, 0, c9, c13, 2)
#define RCP15_PMXEVCNTSR(reg)		MRC15(reg, 0, c9, c15, 1)
#define RCP15_PMXEVTYPER(reg)		MRC15(reg, 0, c9, c13, 1)
#define RCP15_LPM0EVTYPER(reg)		MRC15(reg, 0, c15, c0, 0)
#define RCP15_LPM1EVTYPER(reg)		MRC15(reg, 1, c15, c0, 0)
#define RCP15_LPM2EVTYPER(reg)		MRC15(reg, 2, c15, c0, 0)
#define RCP15_LPM3EVTYPER(reg)		MRC15(reg, 3, c15, c0, 0)
#define RCP15_L2LPMEVTYPER(reg)		MRC15(reg, 3, c15, c2, 0)
#define RCP15_VLPMEVTYPER(reg)		MRC15(reg, 7, c11, c0, 0)
#define RCP15_CONTEXTIDR(reg)		MRC15(reg, 0, c13, c0, 1)
#define RCP15_L2CR0(reg)		MRC15(reg, 3, c15, c0, 1)
#define RCP15_L2VR3F1(reg)		MRC15(reg, 3, c15, c15, 1)

#endif

