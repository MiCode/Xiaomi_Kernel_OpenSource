/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "AfSTMV.h"
#include "AfDef.h"

#define ABS_STMV(x) ((x) < 0 ? -(x) : (x))
#define LC898211_fs 234375

static struct stSmvPar StSmvPar;

void StmvSet(struct stSmvPar StSetSmv)
{
	unsigned char UcSetEnb;
	unsigned char UcSetSwt;
	unsigned short UsParSiz;
	unsigned char UcParItv;
	short SsParStt; /* StepMove Start Position */

	StSmvPar.UsSmvSiz = StSetSmv.UsSmvSiz;
	StSmvPar.UcSmvItv = StSetSmv.UcSmvItv;
	StSmvPar.UcSmvEnb = StSetSmv.UcSmvEnb;

	RegWriteA(AFSEND_211, 0x00); /* StepMove Enable Bit Clear */

	RegReadA(ENBL_211, &UcSetEnb);
	UcSetEnb &= (unsigned char)0xFD;
	RegWriteA(ENBL_211, UcSetEnb); /* Measuremenet Circuit1 Off */

	RegReadA(SWTCH_211, &UcSetSwt);
	UcSetSwt &= (unsigned char)0x7F;
	RegWriteA(SWTCH_211, UcSetSwt); /* RZ1 Switch Cut Off */

	RamReadA(RZ_211H, (unsigned short *)&SsParStt); /* Get Start Position */
	UsParSiz = StSetSmv.UsSmvSiz;			/* Get StepSize */
	UcParItv = StSetSmv.UcSmvItv;			/* Get StepInterval */

	RamWriteA(ms11a_211H, (unsigned short)0x0800);
	RamWriteA(MS1Z22_211H,
		  (unsigned short)SsParStt); /* Set Start Position */
	RamWriteA(MS1Z12_211H, UsParSiz);    /* Set StepSize */
	RegWriteA(STMINT_211, UcParItv);     /* Set StepInterval */

	UcSetSwt |= (unsigned char)0x80;
	RegWriteA(SWTCH_211, UcSetSwt); /* RZ1 Switch ON */
}

unsigned char StmvTo(short SsSmvEnd)
{
	unsigned short UsSmvDpl;
	short SsParStt; /* StepMove Start Position */

	/* PIOA_SetOutput(_PIO_PA29);   // Monitor I/O Port */

	RamReadA(RZ_211H, (unsigned short *)&SsParStt); /* Get Start Position */
	UsSmvDpl = ABS_STMV(SsParStt - SsSmvEnd);

	if ((UsSmvDpl <= StSmvPar.UsSmvSiz) &&
	    ((StSmvPar.UcSmvEnb & STMSV_ON) == STMSV_ON)) {
		if (StSmvPar.UcSmvEnb & STMCHTG_ON)
			RegWriteA(MSSET_211,
				  INI_MSSET_211 | (unsigned char)0x01);

		RamWriteA(MS1Z22_211H,
			  SsSmvEnd); /* Handling Single Step For ES1 */
		StSmvPar.UcSmvEnb |= STMVEN_ON;
	} else {
		if (SsParStt < SsSmvEnd) { /* Check StepMove Direction */
			RamWriteA(MS1Z12_211H, StSmvPar.UsSmvSiz);
		} else if (SsParStt > SsSmvEnd) {
			RamWriteA(MS1Z12_211H, -StSmvPar.UsSmvSiz);
		}

		RamWriteA(STMVENDH_211,
			  SsSmvEnd); /* Set StepMove Target Position */
		StSmvPar.UcSmvEnb |= STMVEN_ON;
		RegWriteA(STMVEN_211, StSmvPar.UcSmvEnb); /* Start StepMove */
	}

	return 0;
}
