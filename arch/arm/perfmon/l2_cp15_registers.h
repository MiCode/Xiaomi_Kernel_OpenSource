/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#ifndef __l2_cp15_registers__
#define __l2_cp15_registers__

#include "mcrmrc.h"

#define WCP15_SDER(reg)			MCR15(reg, 0, c1, c1, 1)
/*
* Performance Monitor Registers
*/
#define WCP15_L2MPCR(reg)		MCR15(reg, 3, c15, c0, 4)
#define WCP15_L2PMCCNTCR(reg)		MCR15(reg, 3, c15, c4, 4)
#define WCP15_L2PMCCNTR(reg)		MCR15(reg, 3, c15, c4, 5)
#define WCP15_L2PMCCNTSR(reg)		MCR15(reg, 3, c15, c4, 6)
#define WCP15_L2PMCNTENCLR(reg)		MCR15(reg, 3, c15, c4, 2)
#define WCP15_L2PMCNTENSET(reg)		MCR15(reg, 3, c15, c4, 3)
#define WCP15_L2PMCR(reg)		MCR15(reg, 3, c15, c4, 0)
#define WCP15_L2PMINTENCLR(reg)		MCR15(reg, 3, c15, c5, 0)
#define WCP15_L2PMINTENSET(reg)		MCR15(reg, 3, c15, c5, 1)
#define WCP15_L2PMOVSR(reg)		MCR15(reg, 3, c15, c4, 1)
#define WCP15_L2PMRLDR(reg)		MCR15(reg, 3, c15, c4, 7)
#define WCP15_L2PMSELR(reg)		MCR15(reg, 3, c15, c6, 0)
#define WCP15_L2PMXEVCNTCR(reg)		MCR15(reg, 3, c15, c6, 4)
#define WCP15_L2PMXEVCNTR(reg)		MCR15(reg, 3, c15, c6, 5)
#define WCP15_L2PMXEVCNTSR(reg)		MCR15(reg, 3, c15, c6, 6)
#define WCP15_L2PMXEVTYPER(reg)		MCR15(reg, 3, c15, c6, 7)
#define WCP15_L2PMXEVFILTER(reg)	MCR15(reg, 3, c15, c6, 3)
#define WCP15_L2PMEVTYPER0(reg)		MCR15(reg, 3, c15, c7, 0)
#define WCP15_L2PMEVTYPER1(reg)		MCR15(reg, 3, c15, c7, 1)
#define WCP15_L2PMEVTYPER2(reg)		MCR15(reg, 3, c15, c7, 2)
#define WCP15_L2PMEVTYPER3(reg)		MCR15(reg, 3, c15, c7, 3)
#define WCP15_L2PMEVTYPER4(reg)		MCR15(reg, 3, c15, c7, 4)
#define WCP15_L2VR3F1(reg)		MCR15(reg, 3, c15, c15, 1)

/*
* READ the registers
*/
#define RCP15_SDER(reg)			MRC15(reg, 0, c1, c1, 1)
/*
* Performance Monitor Registers
*/
#define RCP15_L2MPCR(reg)		MRC15(reg, 3, c15, c0, 4)
#define RCP15_L2PMCCNTCR(reg)		MRC15(reg, 3, c15, c4, 4)
#define RCP15_L2PMCCNTR(reg)		MRC15(reg, 3, c15, c4, 5)
#define RCP15_L2PMCCNTSR(reg)		MRC15(reg, 3, c15, c4, 6)
#define RCP15_L2PMCNTENCLR(reg)		MRC15(reg, 3, c15, c4, 2)
#define RCP15_L2PMCNTENSET(reg)		MRC15(reg, 3, c15, c4, 3)
#define RCP15_L2PMCR(reg)		MRC15(reg, 3, c15, c4, 0)
#define RCP15_L2PMINTENCLR(reg)		MRC15(reg, 3, c15, c5, 0)
#define RCP15_L2PMINTENSET(reg)		MRC15(reg, 3, c15, c5, 1)
#define RCP15_L2PMOVSR(reg)		MRC15(reg, 3, c15, c4, 1)
#define RCP15_L2PMRLDR(reg)		MRC15(reg, 3, c15, c4, 7)
#define RCP15_L2PMSELR(reg)		MRC15(reg, 3, c15, c6, 0)
#define RCP15_L2PMXEVCNTCR(reg)		MRC15(reg, 3, c15, c6, 4)
#define RCP15_L2PMXEVCNTR(reg)		MRC15(reg, 3, c15, c6, 5)
#define RCP15_L2PMXEVCNTSR(reg)		MRC15(reg, 3, c15, c6, 6)
#define RCP15_L2PMXEVTYPER(reg)		MRC15(reg, 3, c15, c6, 7)
#define RCP15_L2PMXEVFILTER(reg)	MRC15(reg, 3, c15, c6, 3)
#define RCP15_L2PMEVTYPER0(reg)		MRC15(reg, 3, c15, c7, 0)
#define RCP15_L2PMEVTYPER1(reg)		MRC15(reg, 3, c15, c7, 1)
#define RCP15_L2PMEVTYPER2(reg)		MRC15(reg, 3, c15, c7, 2)
#define RCP15_L2PMEVTYPER3(reg)		MRC15(reg, 3, c15, c7, 3)
#define RCP15_L2PMEVTYPER4(reg)		MRC15(reg, 3, c15, c7, 4)
#define RCP15_L2VR3F1(reg)		MRC15(reg, 3, c15, c15, 1)

#endif

