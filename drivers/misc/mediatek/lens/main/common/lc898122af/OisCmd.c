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

/* ************************** */
/* Include Header File */
/* ************************** */
#define OISCMD

/* #include      "Main.h" */
/* #include      "Cmd.h" */
#include "Ois.h"
#include "OisDef.h"

struct stAdjPar StAdjPar;  /* Execute Command Parameter */
unsigned char UcOscAdjFlg; /* For Measure trigger */
unsigned long UlH1Coefval; /* H1 coefficient value */
unsigned char UcH1LvlMod;  /* H1 level coef mode */

/* ************************** */
/* define */
/* ************************** */
#define MES_XG1 0 /* LXG1 Measure Mode */
#define MES_XG2 1 /* LXG2 Measure Mode */

#define HALL_ADJ 0
#define LOOPGAIN 1
#define THROUGH 2
#define NOISE 3

/* Measure Mode */

#define TNE 80 /* Waiting Time For Movement */
#ifdef HALLADJ_HW

#define __MEASURE_LOOPGAIN 0x00
#define __MEASURE_BIASOFFSET 0x01

#else

/******* Hall calibration Type 1 *******/
#define MARJIN 0x0300 /* Marjin */
#define BIAS_ADJ_BORDER                                                        \
	0x1998 /* HALL_MAX_GAP < BIAS_ADJ_BORDER < HALL_MIN_GAP(80%) */

#define HALL_MAX_GAP (BIAS_ADJ_BORDER - MARJIN)
#define HALL_MIN_GAP (BIAS_ADJ_BORDER + MARJIN)


#define BIAS_LIMIT 0xFFFF /* HALL BIAS LIMIT */
#define OFFSET_DIV 2      /* Divide Difference For Offset Step */
#define TIME_OUT 40 /* Time Out Count */

/******* Hall calibration Type 2 *******/
#define MARGIN 0x0300 /* Margin */

#define BIAS_ADJ_OVER 0xD998  /* 85% */
#define BIAS_ADJ_RANGE 0xCCCC /* 80% */
#define BIAS_ADJ_SKIP 0xBFFF  /* 75% */
#define HALL_MAX_RANGE (BIAS_ADJ_RANGE + MARGIN)
#define HALL_MIN_RANGE (BIAS_ADJ_RANGE - MARGIN)

#define DECRE_CAL 0x0100 /* decrease value */

#endif

#ifdef H1COEF_CHANGER
#ifdef CORRECT_1DEG
#define MAXLMT 0x40400000  /* 3.0 */
#define MINLMT 0x3FE66666  /* 1.8 */
#define CHGCOEF 0xBA195555 /*  */
#else
#define MAXLMT 0x40000000  /* 2.0 */
#define MINLMT 0x3F8CCCCD  /* 1.1 */
#define CHGCOEF 0xBA4C71C7 /*  */
#endif
#define MINLMT_MOV 0x00000000 /* 0.0 */
#define CHGCOEF_MOV 0xB9700000
#endif

/* ************************** */
/* Global Variable */
/* ************************** */
#ifdef HALLADJ_HW
unsigned char UcAdjBsy;

#else
unsigned short UsStpSiz; /* Bias Step Size */
unsigned short UsErrBia, UsErrOfs;
#endif

/* ************************** */
/* Const */
/* ************************** */
/* gxzoom Setting Value */
#define ZOOMTBL 16
const unsigned long ClGyxZom[ZOOMTBL] = {
	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000,
	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000,
	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000};

/* gyzoom Setting Value */
const unsigned long ClGyyZom[ZOOMTBL] = {
	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000,
	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000,
	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000};

/* DI Coefficient Setting Value */
#define COEFTBL 7
const unsigned long ClDiCof[COEFTBL] = {
	DIFIL_S2, /* 0 */
	DIFIL_S2, /* 1 */
	DIFIL_S2, /* 2 */
	DIFIL_S2, /* 3 */
	DIFIL_S2, /* 4 */
	DIFIL_S2, /* 5 */
	DIFIL_S2  /* 6 */
};


unsigned short TneRun(void)
{
	unsigned char UcHlySts, UcHlxSts, UcAtxSts, UcAtySts;
	unsigned short UsFinSts, UsOscSts; /* Final Adjustment state */
	unsigned char UcDrvMod;
#ifndef HALLADJ_HW
	union UnDwdVal StTneVal;
#endif

#ifdef USE_EXTCLK_ALL /* 24MHz */
	UsOscSts = EXE_END;
#else
#ifdef MODULE_CALIBRATION
	/* OSC adjustment */
	UsOscSts = OscAdj();
#else
	UsOscSts = EXE_END;
#endif
#endif

	UcDrvMod = UcPwmMod;
	if (UcDrvMod == PWMMOD_CVL)
		DrvPwmSw(Mpwm); /* PWM mode */

#ifdef HALLADJ_HW
	UcHlySts = BiasOffsetAdj(Y_DIR, 0);
	WitTim_LC898122AF(TNE);
	UcHlxSts = BiasOffsetAdj(X_DIR, 0);
	WitTim_LC898122AF(TNE);
	UcHlySts = BiasOffsetAdj(Y_DIR, 1);
	WitTim_LC898122AF(TNE);
	UcHlxSts = BiasOffsetAdj(X_DIR, 1);

	SrvCon(Y_DIR, OFF);
	SrvCon(X_DIR, OFF);

	if (UcDrvMod == PWMMOD_CVL)
		DrvPwmSw(Mlnp); /* PWM mode */

#ifdef NEUTRAL_CENTER
	TneHvc();
#endif /* NEUTRAL_CENTER */
#else
	/* StbOnnN( OFF , ON ) ;    */ /* Y OFF, X ON */
	WitTim_LC898122AF(TNE);

	StTneVal.UlDwdVal = TnePtp(Y_DIR, PTP_BEFORE);
	/* UcHlySts        = TneCen( Y_DIR, StTneVal ) ; */
	UcHlySts = TneCen(Y2_DIR, StTneVal);

	StbOnnN(ON, OFF); /* Y ON, X OFF */
	WitTim_LC898122AF(TNE);

	StTneVal.UlDwdVal = TnePtp(X_DIR, PTP_BEFORE);
	/* UcHlxSts        = TneCen( X_DIR, StTneVal ) ; */
	UcHlxSts = TneCen(X2_DIR, StTneVal);

	StbOnnN(OFF, ON); /* Y OFF, X ON */
	WitTim_LC898122AF(TNE);

	StTneVal.UlDwdVal = TnePtp(Y_DIR, PTP_AFTER);
	/* UcHlySts        = TneCen( Y_DIR, StTneVal ) ; */
	UcHlySts = TneCen(Y2_DIR, StTneVal);

	StbOnnN(ON, OFF); /* Y ON, X OFF */
	WitTim_LC898122AF(TNE);

	StTneVal.UlDwdVal = TnePtp(X_DIR, PTP_AFTER);
	/* UcHlxSts        = TneCen( X_DIR, StTneVal ) ; */
	UcHlxSts = TneCen(X2_DIR, StTneVal);

	SrvCon(Y_DIR, OFF);
	SrvCon(X_DIR, OFF);

	if (UcDrvMod == PWMMOD_CVL)
		DrvPwmSw(Mlnp); /* PWM mode */

#ifdef NEUTRAL_CENTER
	TneHvc();
#endif /* NEUTRAL_CENTER */
#endif

	WitTim_LC898122AF(TNE);

	RamAccFixMod(ON); /* Fix mode */

	StAdjPar.StHalAdj.UsAdxOff =
		(unsigned short)(0x00010000 -
				 (unsigned long)StAdjPar.StHalAdj.UsHlxCna);
	StAdjPar.StHalAdj.UsAdyOff =
		(unsigned short)(0x00010000 -
				 (unsigned long)StAdjPar.StHalAdj.UsHlyCna);

	RamWriteA_LC898122AF(OFF0Z, StAdjPar.StHalAdj.UsAdxOff); /* 0x1450 */
	RamWriteA_LC898122AF(OFF1Z, StAdjPar.StHalAdj.UsAdyOff); /* 0x14D0 */

	RamReadA_LC898122AF(DAXHLO, &StAdjPar.StHalAdj.UsHlxOff); /* 0x1479 */
	RamReadA_LC898122AF(DAXHLB, &StAdjPar.StHalAdj.UsHlxGan); /* 0x147A */
	RamReadA_LC898122AF(DAYHLO, &StAdjPar.StHalAdj.UsHlyOff); /* 0x14F9 */
	RamReadA_LC898122AF(DAYHLB, &StAdjPar.StHalAdj.UsHlyGan); /* 0x14FA */
	RamReadA_LC898122AF(OFF0Z, &StAdjPar.StHalAdj.UsAdxOff);  /* 0x1450 */
	RamReadA_LC898122AF(OFF1Z, &StAdjPar.StHalAdj.UsAdyOff);  /* 0x14D0 */

	RamAccFixMod(OFF); /* Float mode */

	StbOnn(); /* Slope Mode */

	WitTim_LC898122AF(TNE);

#ifdef MODULE_CALIBRATION
	/* X Loop Gain Adjust */
	UcAtxSts = LopGan(X_DIR);

	/* Y Loop Gain Adjust */
	UcAtySts = LopGan(Y_DIR);
#else /* default value */
	RamAccFixMod(ON);				       /* Fix mode */
	RamReadA_LC898122AF(sxg, &StAdjPar.StLopGan.UsLxgVal); /* 0x10D3 */
	RamReadA_LC898122AF(syg, &StAdjPar.StLopGan.UsLygVal); /* 0x11D3 */
	RamAccFixMod(OFF);				       /* Float mode */
	UcAtxSts = EXE_END;
	UcAtySts = EXE_END;
#endif

	TneGvc();

	UsFinSts = (unsigned short)(UcHlxSts - EXE_END) +
		   (unsigned short)(UcHlySts - EXE_END) +
		   (unsigned short)(UcAtxSts - EXE_END) +
		   (unsigned short)(UcAtySts - EXE_END) +
		   (UsOscSts - (unsigned short)EXE_END) +
		   (unsigned short)EXE_END;

	return UsFinSts;
}

#ifndef HALLADJ_HW


unsigned long TnePtp(unsigned char UcDirSel, unsigned char UcBfrAft)
{
	union UnDwdVal StTneVal;

	MesFil(THROUGH); /* 測定用フィ?ターを設定する。 */

	if (!UcDirSel) {
		RamWrite32A_LC898122AF(sxsin, HALL_H_VAL); /* 0x10D5 */
		SetSinWavePara(0x0A, XHALWAVE);
	} else {
		RamWrite32A_LC898122AF(sysin, HALL_H_VAL); /* 0x11D5 */
		SetSinWavePara(0x0A, YHALWAVE);
	}

	if (!UcDirSel) { /* AXIS X */
		RegWriteA_LC898122AF(WC_MES1ADD0,
				     (unsigned char)AD0Z); /* 0x0194       */
		RegWriteA_LC898122AF(
			WC_MES1ADD1,
			(unsigned char)((AD0Z >> 8) & 0x0001)); /* 0x0195 */
	} else { /* AXIS Y */
		RegWriteA_LC898122AF(WC_MES1ADD0,
				     (unsigned char)AD1Z); /* 0x0194       */
		RegWriteA_LC898122AF(
			WC_MES1ADD1,
			(unsigned char)((AD1Z >> 8) & 0x0001)); /* 0x0195 */
	}

	RegWriteA_LC898122AF(WC_MESLOOP1,
			     0x00); /* 0x0193       CmMesLoop[15:8] */
	RegWriteA_LC898122AF(WC_MESLOOP0,
			     0x01); /* 0x0192       CmMesLoop[7:0] */

	RamWrite32A_LC898122AF(msmean,
			       0x3F800000); /* 0x1230       1/CmMesLoop[15:0] */

	RamWrite32A_LC898122AF(MSMAX1, 0x00000000);   /* 0x1050 */
	RamWrite32A_LC898122AF(MSMAX1AV, 0x00000000); /* 0x1051 */
	RamWrite32A_LC898122AF(MSMIN1, 0x00000000);   /* 0x1060 */
	RamWrite32A_LC898122AF(MSMIN1AV, 0x00000000); /* 0x1061 */

	RegWriteA_LC898122AF(WC_MESABS, 0x00); /* 0x0198       none ABS */
	BsyWit(WC_MESMODE, 0x02); /* 0x0190               Sine wave Measure */

	RamAccFixMod(ON); /* Fix mode */

	RamReadA_LC898122AF(MSMAX1AV, &StTneVal.StDwdVal.UsHigVal); /* 0x1051 */
	RamReadA_LC898122AF(MSMIN1AV, &StTneVal.StDwdVal.UsLowVal); /* 0x1061 */

	RamAccFixMod(OFF); /* Float mode */

	if (!UcDirSel) {			/* AXIS X */
		SetSinWavePara(0x00, XHALWAVE); /* STOP */
	} else {
		SetSinWavePara(0x00, YHALWAVE); /* STOP */
	}

	if (UcBfrAft == 0) {
		if (UcDirSel == X_DIR) {
			StAdjPar.StHalAdj.UsHlxCen =
				((signed short)StTneVal.StDwdVal.UsHigVal +
				 (signed short)StTneVal.StDwdVal.UsLowVal) /
				2;
			StAdjPar.StHalAdj.UsHlxMax = StTneVal.StDwdVal.UsHigVal;
			StAdjPar.StHalAdj.UsHlxMin = StTneVal.StDwdVal.UsLowVal;
		} else {
			StAdjPar.StHalAdj.UsHlyCen =
				((signed short)StTneVal.StDwdVal.UsHigVal +
				 (signed short)StTneVal.StDwdVal.UsLowVal) /
				2;
			StAdjPar.StHalAdj.UsHlyMax = StTneVal.StDwdVal.UsHigVal;
			StAdjPar.StHalAdj.UsHlyMin = StTneVal.StDwdVal.UsLowVal;
		}
	} else {
		if (UcDirSel == X_DIR) {
			StAdjPar.StHalAdj.UsHlxCna =
				((signed short)StTneVal.StDwdVal.UsHigVal +
				 (signed short)StTneVal.StDwdVal.UsLowVal) /
				2;
			StAdjPar.StHalAdj.UsHlxMxa = StTneVal.StDwdVal.UsHigVal;
			StAdjPar.StHalAdj.UsHlxMna = StTneVal.StDwdVal.UsLowVal;
		} else {
			StAdjPar.StHalAdj.UsHlyCna =
				((signed short)StTneVal.StDwdVal.UsHigVal +
				 (signed short)StTneVal.StDwdVal.UsLowVal) /
				2;
			StAdjPar.StHalAdj.UsHlyMxa = StTneVal.StDwdVal.UsHigVal;
			StAdjPar.StHalAdj.UsHlyMna = StTneVal.StDwdVal.UsLowVal;
		}
	}

	StTneVal.StDwdVal.UsHigVal = 0x7fff - StTneVal.StDwdVal.UsHigVal;
	/* Maximum Gap = Maximum - Hall Peak Top */
	StTneVal.StDwdVal.UsLowVal = StTneVal.StDwdVal.UsLowVal - 0x7fff;
	/* Minimum Gap = Hall Peak Bottom - Minimum */

	return StTneVal.UlDwdVal;
}

unsigned short UsValBef, UsValNow;
unsigned char TneCen(unsigned char UcTneAxs, union UnDwdVal StTneVal)
{
	unsigned char UcTneRst, UcTmeOut, UcTofRst;
	unsigned short UsOffDif;
	unsigned short UsBiasVal;

	UsErrBia = 0;
	UsErrOfs = 0;
	UcTmeOut = 1;
	UsStpSiz = 1;
	UcTneRst = FAILURE;
	UcTofRst = FAILURE;

	while (UcTneRst && UcTmeOut) {
		if (UcTofRst == FAILURE) {
			StTneVal.UlDwdVal = TneOff(StTneVal, UcTneAxs);
		} else {
			StTneVal.UlDwdVal = TneBia(StTneVal, UcTneAxs);
			UcTofRst = FAILURE;
		}

		if (!(UcTneAxs & 0xF0)) {
			if (StTneVal.StDwdVal.UsHigVal >
			    StTneVal.StDwdVal
				    .UsLowVal) {
				UsOffDif = (StTneVal.StDwdVal.UsHigVal -
					    StTneVal.StDwdVal.UsLowVal) /
					   2;
			} else {
				UsOffDif = (StTneVal.StDwdVal.UsLowVal -
					    StTneVal.StDwdVal.UsHigVal) /
					   2;
			}

			if (UsOffDif < MARJIN)
				UcTofRst = SUCCESS;
			else
				UcTofRst = FAILURE;

			/* Check Tuning Result */
			if ((StTneVal.StDwdVal.UsHigVal < HALL_MIN_GAP &&
			     StTneVal.StDwdVal.UsLowVal < HALL_MIN_GAP) &&
			    (StTneVal.StDwdVal.UsHigVal > HALL_MAX_GAP &&
			     StTneVal.StDwdVal.UsLowVal > HALL_MAX_GAP)) {
				UcTneRst = SUCCESS;
				break;
			} else if (UsStpSiz == 0) {
				UcTneRst = SUCCESS;
				break;
			}

			UcTneRst = FAILURE;
			UcTmeOut++;
		} else {
			if ((StTneVal.StDwdVal.UsHigVal > MARGIN) &&
			    (StTneVal.StDwdVal.UsLowVal > MARGIN)) {
				UcTofRst = SUCCESS;
				UsValBef = UsValNow = 0x0000;
			} else if ((StTneVal.StDwdVal.UsHigVal <= MARGIN) &&
				   (StTneVal.StDwdVal.UsLowVal <= MARGIN)) {
				UcTofRst = SUCCESS;
				UcTneRst = FAILURE;
			} else if (((unsigned short)0xFFFF -
				    (StTneVal.StDwdVal.UsHigVal +
				     StTneVal.StDwdVal.UsLowVal)) >
				   BIAS_ADJ_OVER) {
				UcTofRst = SUCCESS;
				UcTneRst = FAILURE;
			} else {
				UcTofRst = FAILURE;

				UsValBef = UsValNow;

				RamAccFixMod(ON); /* Fix mode */

				if (!(UcTneAxs & 0x0F))
					RamReadA_LC898122AF(
						DAXHLO, &UsValNow);
				else
					RamReadA_LC898122AF(
						DAYHLO, &UsValNow);

				if ((((UsValBef & 0xFF00) == 0x8000) &&
				     (UsValNow & 0xFF00) == 0x8000) ||
				    (((UsValBef & 0xFF00) == 0x7F00) &&
				     (UsValNow & 0xFF00) == 0x7F00)) {
					if (!(UcTneAxs & 0x0F))
						RamReadA_LC898122AF(DAXHLB,
								    &UsBiasVal);
					else
						RamReadA_LC898122AF(DAYHLB,
								    &UsBiasVal);

					if (UsBiasVal > 0x8000)
						UsBiasVal -= 0x8000;
					else
						UsBiasVal += 0x8000;

					if (UsBiasVal > DECRE_CAL)
						UsBiasVal -= DECRE_CAL;

					UsBiasVal += 0x8000;

					if (!(UcTneAxs & 0x0F))
						RamWriteA_LC898122AF(DAXHLB,
								     UsBiasVal);
					else
						RamWriteA_LC898122AF(DAYHLB,
								     UsBiasVal);
				}

				RamAccFixMod(OFF); /* Float mode */
			}

			if ((((unsigned short)0xFFFF -
			      (StTneVal.StDwdVal.UsHigVal +
			       StTneVal.StDwdVal.UsLowVal)) < HALL_MAX_RANGE) &&
			    (((unsigned short)0xFFFF -
			      (StTneVal.StDwdVal.UsHigVal +
			       StTneVal.StDwdVal.UsLowVal)) > HALL_MIN_RANGE)) {
				if (UcTofRst == SUCCESS) {
					UcTneRst = SUCCESS;
					break;
				}
			}
			UcTneRst = FAILURE;
			UcTmeOut++;
		}

		if (UcTneAxs & 0xF0) {
			if ((UcTmeOut / 2) == TIME_OUT)
				UcTmeOut = 0;
			/* Set Time Out Count */
		} else {
			if (UcTmeOut == TIME_OUT)
				UcTmeOut = 0;
			/* Set Time Out Count */
		}
	}

	if (UcTneRst == FAILURE) {
		if (!(UcTneAxs & 0x0F)) {
			UcTneRst = EXE_HXADJ;
			StAdjPar.StHalAdj.UsHlxGan = 0xFFFF;
			StAdjPar.StHalAdj.UsHlxOff = 0xFFFF;
		} else {
			UcTneRst = EXE_HYADJ;
			StAdjPar.StHalAdj.UsHlyGan = 0xFFFF;
			StAdjPar.StHalAdj.UsHlyOff = 0xFFFF;
		}
	} else {
		UcTneRst = EXE_END;
	}

	return UcTneRst;
}

unsigned long TneBia(union UnDwdVal StTneVal, unsigned char UcTneAxs)
{
	long SlSetBia;
	unsigned short UsSetBia;
	unsigned char UcChkFst;
	static unsigned short UsTneVax; /* Variable For 1/2 Searching */
	unsigned short UsDecCal;

	UcChkFst = 1;

	if (UsStpSiz == 1) {
		UsTneVax = 2;

		if (UcTneAxs & 0xF0) {
			if (((unsigned short)0xFFFF -
			     (StTneVal.StDwdVal.UsHigVal +
			      StTneVal.StDwdVal.UsLowVal)) > BIAS_ADJ_OVER) {
				UcChkFst = 0;

				RamAccFixMod(ON); /* Fix mode */

				if (!(UcTneAxs &
				      0x0F)) {
					RamReadA_LC898122AF(
						DAXHLB, &UsSetBia);
				} else {
					RamReadA_LC898122AF(
						DAYHLB, &UsSetBia);
				}
				if (UsSetBia > 0x8000)
					UsSetBia -= 0x8000;
				else
					UsSetBia += 0x8000;

				if (!UcChkFst)
					UsDecCal = (DECRE_CAL << 3);
				else
					UsDecCal = DECRE_CAL;

				if (UsSetBia > UsDecCal)
					UsSetBia -= UsDecCal;

				UsSetBia += 0x8000;
				if (!(UcTneAxs &
				      0x0F)) {
					RamWriteA_LC898122AF(
						DAXHLB, UsSetBia);
					RamWriteA_LC898122AF(
						DAXHLO, 0x0000);
				} else {
					RamWriteA_LC898122AF(
						DAYHLB, UsSetBia);
					RamWriteA_LC898122AF(
						DAYHLO, 0x0000);
				}
				UsStpSiz = BIAS_LIMIT / UsTneVax;

				RamAccFixMod(OFF); /* Float mode */
			}
		} else {
			if ((StTneVal.StDwdVal.UsHigVal +
			     StTneVal.StDwdVal.UsLowVal) /
				    2 <
			    BIAS_ADJ_BORDER) {
				UcChkFst = 0;
			}

			if (!UcTneAxs) {

				RamWrite32A_LC898122AF(
					DAXHLB, 0xBF800000);
				RamWrite32A_LC898122AF(
					DAXHLO, 0x00000000);

				UsStpSiz = BIAS_LIMIT / UsTneVax;
			} else {
				RamWrite32A_LC898122AF(
					DAYHLB, 0xBF800000);
				RamWrite32A_LC898122AF(
					DAYHLO,
					0x00000000);
				UsStpSiz = BIAS_LIMIT / UsTneVax;
			}
		}
	}

	RamAccFixMod(ON); /* Fix mode */

	if (!(UcTneAxs & 0x0F)) {
		RamReadA_LC898122AF(
			DAXHLB, &UsSetBia);
		SlSetBia = (long)UsSetBia;
	} else {
		RamReadA_LC898122AF(
			DAYHLB, &UsSetBia);
		SlSetBia = (long)UsSetBia;
	}

	if (SlSetBia >= 0x00008000)
		SlSetBia |= 0xFFFF0000;

	if (UcChkFst) {
		if (UcTneAxs & 0xF0) {
			/* Calculatiton For Hall BIAS 1/2 Searching */
			if (((unsigned short)0xFFFF -
			     (StTneVal.StDwdVal.UsHigVal +
			      StTneVal.StDwdVal.UsLowVal)) < BIAS_ADJ_RANGE) {
				if (((unsigned short)0xFFFF -
				     (StTneVal.StDwdVal.UsHigVal +
				      StTneVal.StDwdVal.UsLowVal)) <
				    BIAS_ADJ_SKIP) {
					SlSetBia += 0x0400;
				} else {
					SlSetBia += 0x0100;
				}
			} else {
				if (((unsigned short)0xFFFF -
				     (StTneVal.StDwdVal.UsHigVal +
				      StTneVal.StDwdVal.UsLowVal)) >
				    BIAS_ADJ_OVER) {
					SlSetBia -= 0x0400;
				} else {
					SlSetBia -= 0x0100;
				}
			}
			UsStpSiz = 0x0200;

		} else {
			/* Calculatiton For Hall BIAS 1/2 Searching */
			if ((StTneVal.StDwdVal.UsHigVal +
			     StTneVal.StDwdVal.UsLowVal) /
				    2 >
			    BIAS_ADJ_BORDER)
				SlSetBia += UsStpSiz;
			else
				SlSetBia -= UsStpSiz;

			UsTneVax = UsTneVax * 2;
			UsStpSiz = BIAS_LIMIT / UsTneVax;
		}
	}

	if (SlSetBia > 0x00007FFF)
		SlSetBia = 0x00007FFF;
	else if (SlSetBia < 0xFFFF8001)
		SlSetBia = 0xFFFF8001;

	if (!(UcTneAxs & 0x0F))
		RamWriteA_LC898122AF(
			DAXHLB,
			SlSetBia);
	else
		RamWriteA_LC898122AF(
			DAYHLB,
			SlSetBia);

	RamAccFixMod(OFF); /* Float mode */

	StTneVal.UlDwdVal = TnePtp(UcTneAxs & 0x0F, PTP_AFTER);

	return StTneVal.UlDwdVal;
}

unsigned long TneOff(union UnDwdVal StTneVal, unsigned char UcTneAxs)
{
	long SlSetOff;
	unsigned short UsSetOff;

	UcTneAxs &= 0x0F;

	RamAccFixMod(ON); /* Fix mode */

	if (!UcTneAxs) {
		RamReadA_LC898122AF(
			DAXHLO,
			&UsSetOff);
		SlSetOff = (long)UsSetOff;
	} else {
		RamReadA_LC898122AF(
			DAYHLO,
			&UsSetOff);
		SlSetOff = (long)UsSetOff;
	}

	if (SlSetOff > 0x00008000)
		SlSetOff |= 0xFFFF0000;

	if (StTneVal.StDwdVal.UsHigVal > StTneVal.StDwdVal.UsLowVal) {
		SlSetOff += (StTneVal.StDwdVal.UsHigVal -
			     StTneVal.StDwdVal.UsLowVal) /
			    OFFSET_DIV;
		/* Calculating Value For Increase Step */
	} else {
		SlSetOff -= (StTneVal.StDwdVal.UsLowVal -
			     StTneVal.StDwdVal.UsHigVal) /
			    OFFSET_DIV;
		/* Calculating Value For Decrease Step */
	}

	if (SlSetOff > 0x00007FFF)
		SlSetOff = 0x00007FFF;
	else if (SlSetOff < 0xFFFF8001)
		SlSetOff = 0xFFFF8001;

	if (!UcTneAxs)
		RamWriteA_LC898122AF(
			DAXHLO,
			SlSetOff);
	else
		RamWriteA_LC898122AF(
			DAYHLO,
			SlSetOff);

	RamAccFixMod(OFF); /* Float mode */

	StTneVal.UlDwdVal = TnePtp(UcTneAxs, PTP_AFTER);

	return StTneVal.UlDwdVal;
}

#endif

void MesFil(unsigned char UcMesMod)
{
#ifdef USE_EXTCLK_ALL    /* 24MHz */
	if (!UcMesMod) { /* Hall Bias&Offset Adjust */
		/* Measure Filter1 Setting */
		RamWrite32A_LC898122AF(mes1aa,
				       0x3D1E5A40); /* 0x10F0       LPF150Hz */
		RamWrite32A_LC898122AF(mes1ab, 0x3D1E5A40); /* 0x10F1 */
		RamWrite32A_LC898122AF(mes1ac, 0x3F6C34C0); /* 0x10F2 */
		RamWrite32A_LC898122AF(mes1ad, 0x00000000); /* 0x10F3 */
		RamWrite32A_LC898122AF(mes1ae, 0x00000000); /* 0x10F4 */
		RamWrite32A_LC898122AF(mes1ba, 0x3F800000); /* 0x10F5 Through */
		RamWrite32A_LC898122AF(mes1bb, 0x00000000); /* 0x10F6 */
		RamWrite32A_LC898122AF(mes1bc, 0x00000000); /* 0x10F7 */
		RamWrite32A_LC898122AF(mes1bd, 0x00000000); /* 0x10F8 */
		RamWrite32A_LC898122AF(mes1be, 0x00000000); /* 0x10F9 */

		/* Measure Filter2 Setting */
		RamWrite32A_LC898122AF(mes2aa,
				       0x3D1E5A40); /* 0x11F0       LPF150Hz */
		RamWrite32A_LC898122AF(mes2ab, 0x3D1E5A40); /* 0x11F1 */
		RamWrite32A_LC898122AF(mes2ac, 0x3F6C34C0); /* 0x11F2 */
		RamWrite32A_LC898122AF(mes2ad, 0x00000000); /* 0x11F3 */
		RamWrite32A_LC898122AF(mes2ae, 0x00000000); /* 0x11F4 */
		RamWrite32A_LC898122AF(mes2ba, 0x3F800000); /* 0x11F5 Through */
		RamWrite32A_LC898122AF(mes2bb, 0x00000000); /* 0x11F6 */
		RamWrite32A_LC898122AF(mes2bc, 0x00000000); /* 0x11F7 */
		RamWrite32A_LC898122AF(mes2bd, 0x00000000); /* 0x11F8 */
		RamWrite32A_LC898122AF(mes2be, 0x00000000); /* 0x11F9 */

	} else if (UcMesMod == LOOPGAIN) { /* Loop Gain Adjust */
		/* Measure Filter1 Setting */
		RamWrite32A_LC898122AF(mes1aa,
				       0x3E587E00); /* 0x10F0       LPF1000Hz */
		RamWrite32A_LC898122AF(mes1ab, 0x3E587E00); /* 0x10F1 */
		RamWrite32A_LC898122AF(mes1ac, 0x3F13C100); /* 0x10F2 */
		RamWrite32A_LC898122AF(mes1ad, 0x00000000); /* 0x10F3 */
		RamWrite32A_LC898122AF(mes1ae, 0x00000000); /* 0x10F4 */
		RamWrite32A_LC898122AF(mes1ba, 0x3F7DF500); /* 0x10F5 HPF30Hz */
		RamWrite32A_LC898122AF(mes1bb, 0xBF7DF500); /* 0x10F6 */
		RamWrite32A_LC898122AF(mes1bc, 0x3F7BEA40); /* 0x10F7 */
		RamWrite32A_LC898122AF(mes1bd, 0x00000000); /* 0x10F8 */
		RamWrite32A_LC898122AF(mes1be, 0x00000000); /* 0x10F9 */

		/* Measure Filter2 Setting */
		RamWrite32A_LC898122AF(mes2aa,
				       0x3E587E00); /* 0x11F0       LPF1000Hz */
		RamWrite32A_LC898122AF(mes2ab, 0x3E587E00); /* 0x11F1 */
		RamWrite32A_LC898122AF(mes2ac, 0x3F13C100); /* 0x11F2 */
		RamWrite32A_LC898122AF(mes2ad, 0x00000000); /* 0x11F3 */
		RamWrite32A_LC898122AF(mes2ae, 0x00000000); /* 0x11F4 */
		RamWrite32A_LC898122AF(mes2ba, 0x3F7DF500); /* 0x11F5 HPF30Hz */
		RamWrite32A_LC898122AF(mes2bb, 0xBF7DF500); /* 0x11F6 */
		RamWrite32A_LC898122AF(mes2bc, 0x3F7BEA40); /* 0x11F7 */
		RamWrite32A_LC898122AF(mes2bd, 0x00000000); /* 0x11F8 */
		RamWrite32A_LC898122AF(mes2be, 0x00000000); /* 0x11F9 */

	} else if (UcMesMod == THROUGH) { /* for Through */
		/* Measure Filter1 Setting */
		RamWrite32A_LC898122AF(mes1aa, 0x3F800000); /* 0x10F0 Through */
		RamWrite32A_LC898122AF(mes1ab, 0x00000000); /* 0x10F1 */
		RamWrite32A_LC898122AF(mes1ac, 0x00000000); /* 0x10F2 */
		RamWrite32A_LC898122AF(mes1ad, 0x00000000); /* 0x10F3 */
		RamWrite32A_LC898122AF(mes1ae, 0x00000000); /* 0x10F4 */
		RamWrite32A_LC898122AF(mes1ba, 0x3F800000); /* 0x10F5 Through */
		RamWrite32A_LC898122AF(mes1bb, 0x00000000); /* 0x10F6 */
		RamWrite32A_LC898122AF(mes1bc, 0x00000000); /* 0x10F7 */
		RamWrite32A_LC898122AF(mes1bd, 0x00000000); /* 0x10F8 */
		RamWrite32A_LC898122AF(mes1be, 0x00000000); /* 0x10F9 */

		/* Measure Filter2 Setting */
		RamWrite32A_LC898122AF(mes2aa, 0x3F800000); /* 0x11F0 Through */
		RamWrite32A_LC898122AF(mes2ab, 0x00000000); /* 0x11F1 */
		RamWrite32A_LC898122AF(mes2ac, 0x00000000); /* 0x11F2 */
		RamWrite32A_LC898122AF(mes2ad, 0x00000000); /* 0x11F3 */
		RamWrite32A_LC898122AF(mes2ae, 0x00000000); /* 0x11F4 */
		RamWrite32A_LC898122AF(mes2ba, 0x3F800000); /* 0x11F5 Through */
		RamWrite32A_LC898122AF(mes2bb, 0x00000000); /* 0x11F6 */
		RamWrite32A_LC898122AF(mes2bc, 0x00000000); /* 0x11F7 */
		RamWrite32A_LC898122AF(mes2bd, 0x00000000); /* 0x11F8 */
		RamWrite32A_LC898122AF(mes2be, 0x00000000); /* 0x11F9 */

	} else if (UcMesMod == NOISE) { /* SINE WAVE TEST for NOISE */
		/* Measure Filter1 Setting */
		RamWrite32A_LC898122AF(mes1aa,
				       0x3D1E5A40); /* 0x10F0       LPF150Hz */
		RamWrite32A_LC898122AF(mes1ab, 0x3D1E5A40); /* 0x10F1 */
		RamWrite32A_LC898122AF(mes1ac, 0x3F6C34C0); /* 0x10F2 */
		RamWrite32A_LC898122AF(mes1ad, 0x00000000); /* 0x10F3 */
		RamWrite32A_LC898122AF(mes1ae, 0x00000000); /* 0x10F4 */
		RamWrite32A_LC898122AF(mes1ba,
				       0x3D1E5A40); /* 0x10F5       LPF150Hz */
		RamWrite32A_LC898122AF(mes1bb, 0x3D1E5A40); /* 0x10F6 */
		RamWrite32A_LC898122AF(mes1bc, 0x3F6C34C0); /* 0x10F7 */
		RamWrite32A_LC898122AF(mes1bd, 0x00000000); /* 0x10F8 */
		RamWrite32A_LC898122AF(mes1be, 0x00000000); /* 0x10F9 */

		/* Measure Filter2 Setting */
		RamWrite32A_LC898122AF(mes2aa,
				       0x3D1E5A40); /* 0x11F0       LPF150Hz */
		RamWrite32A_LC898122AF(mes2ab, 0x3D1E5A40); /* 0x11F1 */
		RamWrite32A_LC898122AF(mes2ac, 0x3F6C34C0); /* 0x11F2 */
		RamWrite32A_LC898122AF(mes2ad, 0x00000000); /* 0x11F3 */
		RamWrite32A_LC898122AF(mes2ae, 0x00000000); /* 0x11F4 */
		RamWrite32A_LC898122AF(mes2ba,
				       0x3D1E5A40); /* 0x11F5       LPF150Hz */
		RamWrite32A_LC898122AF(mes2bb, 0x3D1E5A40); /* 0x11F6 */
		RamWrite32A_LC898122AF(mes2bc, 0x3F6C34C0); /* 0x11F7 */
		RamWrite32A_LC898122AF(mes2bd, 0x00000000); /* 0x11F8 */
		RamWrite32A_LC898122AF(mes2be, 0x00000000); /* 0x11F9 */
	}
#else
	if (!UcMesMod) { /* Hall Bias&Offset Adjust */
		/* Measure Filter1 Setting */
		RamWrite32A_LC898122AF(mes1aa,
				       0x3CA175C0); /* 0x10F0       LPF150Hz */
		RamWrite32A_LC898122AF(mes1ab, 0x3CA175C0); /* 0x10F1 */
		RamWrite32A_LC898122AF(mes1ac, 0x3F75E8C0); /* 0x10F2 */
		RamWrite32A_LC898122AF(mes1ad, 0x00000000); /* 0x10F3 */
		RamWrite32A_LC898122AF(mes1ae, 0x00000000); /* 0x10F4 */
		RamWrite32A_LC898122AF(mes1ba, 0x3F800000); /* 0x10F5 Through */
		RamWrite32A_LC898122AF(mes1bb, 0x00000000); /* 0x10F6 */
		RamWrite32A_LC898122AF(mes1bc, 0x00000000); /* 0x10F7 */
		RamWrite32A_LC898122AF(mes1bd, 0x00000000); /* 0x10F8 */
		RamWrite32A_LC898122AF(mes1be, 0x00000000); /* 0x10F9 */

		/* Measure Filter2 Setting */
		RamWrite32A_LC898122AF(mes2aa,
				       0x3CA175C0); /* 0x11F0       LPF150Hz */
		RamWrite32A_LC898122AF(mes2ab, 0x3CA175C0); /* 0x11F1 */
		RamWrite32A_LC898122AF(mes2ac, 0x3F75E8C0); /* 0x11F2 */
		RamWrite32A_LC898122AF(mes2ad, 0x00000000); /* 0x11F3 */
		RamWrite32A_LC898122AF(mes2ae, 0x00000000); /* 0x11F4 */
		RamWrite32A_LC898122AF(mes2ba, 0x3F800000); /* 0x11F5 Through */
		RamWrite32A_LC898122AF(mes2bb, 0x00000000); /* 0x11F6 */
		RamWrite32A_LC898122AF(mes2bc, 0x00000000); /* 0x11F7 */
		RamWrite32A_LC898122AF(mes2bd, 0x00000000); /* 0x11F8 */
		RamWrite32A_LC898122AF(mes2be, 0x00000000); /* 0x11F9 */

	} else if (UcMesMod == LOOPGAIN) { /* Loop Gain Adjust */
		/* Measure Filter1 Setting */
		RamWrite32A_LC898122AF(mes1aa,
				       0x3DF21080); /* 0x10F0       LPF1000Hz */
		RamWrite32A_LC898122AF(mes1ab, 0x3DF21080); /* 0x10F1 */
		RamWrite32A_LC898122AF(mes1ac, 0x3F437BC0); /* 0x10F2 */
		RamWrite32A_LC898122AF(mes1ad, 0x00000000); /* 0x10F3 */
		RamWrite32A_LC898122AF(mes1ae, 0x00000000); /* 0x10F4 */
		RamWrite32A_LC898122AF(mes1ba, 0x3F7EF980); /* 0x10F5 HPF30Hz */
		RamWrite32A_LC898122AF(mes1bb, 0xBF7EF980); /* 0x10F6 */
		RamWrite32A_LC898122AF(mes1bc, 0x3F7DF300); /* 0x10F7 */
		RamWrite32A_LC898122AF(mes1bd, 0x00000000); /* 0x10F8 */
		RamWrite32A_LC898122AF(mes1be, 0x00000000); /* 0x10F9 */

		/* Measure Filter2 Setting */
		RamWrite32A_LC898122AF(mes2aa,
				       0x3DF21080); /* 0x11F0       LPF1000Hz */
		RamWrite32A_LC898122AF(mes2ab, 0x3DF21080); /* 0x11F1 */
		RamWrite32A_LC898122AF(mes2ac, 0x3F437BC0); /* 0x11F2 */
		RamWrite32A_LC898122AF(mes2ad, 0x00000000); /* 0x11F3 */
		RamWrite32A_LC898122AF(mes2ae, 0x00000000); /* 0x11F4 */
		RamWrite32A_LC898122AF(mes2ba, 0x3F7EF980); /* 0x11F5 HPF30Hz */
		RamWrite32A_LC898122AF(mes2bb, 0xBF7EF980); /* 0x11F6 */
		RamWrite32A_LC898122AF(mes2bc, 0x3F7DF300); /* 0x11F7 */
		RamWrite32A_LC898122AF(mes2bd, 0x00000000); /* 0x11F8 */
		RamWrite32A_LC898122AF(mes2be, 0x00000000); /* 0x11F9 */

	} else if (UcMesMod == THROUGH) { /* for Through */
		/* Measure Filter1 Setting */
		RamWrite32A_LC898122AF(mes1aa, 0x3F800000); /* 0x10F0 Through */
		RamWrite32A_LC898122AF(mes1ab, 0x00000000); /* 0x10F1 */
		RamWrite32A_LC898122AF(mes1ac, 0x00000000); /* 0x10F2 */
		RamWrite32A_LC898122AF(mes1ad, 0x00000000); /* 0x10F3 */
		RamWrite32A_LC898122AF(mes1ae, 0x00000000); /* 0x10F4 */
		RamWrite32A_LC898122AF(mes1ba, 0x3F800000); /* 0x10F5 Through */
		RamWrite32A_LC898122AF(mes1bb, 0x00000000); /* 0x10F6 */
		RamWrite32A_LC898122AF(mes1bc, 0x00000000); /* 0x10F7 */
		RamWrite32A_LC898122AF(mes1bd, 0x00000000); /* 0x10F8 */
		RamWrite32A_LC898122AF(mes1be, 0x00000000); /* 0x10F9 */

		/* Measure Filter2 Setting */
		RamWrite32A_LC898122AF(mes2aa, 0x3F800000); /* 0x11F0 Through */
		RamWrite32A_LC898122AF(mes2ab, 0x00000000); /* 0x11F1 */
		RamWrite32A_LC898122AF(mes2ac, 0x00000000); /* 0x11F2 */
		RamWrite32A_LC898122AF(mes2ad, 0x00000000); /* 0x11F3 */
		RamWrite32A_LC898122AF(mes2ae, 0x00000000); /* 0x11F4 */
		RamWrite32A_LC898122AF(mes2ba, 0x3F800000); /* 0x11F5 Through */
		RamWrite32A_LC898122AF(mes2bb, 0x00000000); /* 0x11F6 */
		RamWrite32A_LC898122AF(mes2bc, 0x00000000); /* 0x11F7 */
		RamWrite32A_LC898122AF(mes2bd, 0x00000000); /* 0x11F8 */
		RamWrite32A_LC898122AF(mes2be, 0x00000000); /* 0x11F9 */

	} else if (UcMesMod == NOISE) { /* SINE WAVE TEST for NOISE */
		/* Measure Filter1 Setting */
		RamWrite32A_LC898122AF(mes1aa,
				       0x3CA175C0); /* 0x10F0       LPF150Hz */
		RamWrite32A_LC898122AF(mes1ab, 0x3CA175C0); /* 0x10F1 */
		RamWrite32A_LC898122AF(mes1ac, 0x3F75E8C0); /* 0x10F2 */
		RamWrite32A_LC898122AF(mes1ad, 0x00000000); /* 0x10F3 */
		RamWrite32A_LC898122AF(mes1ae, 0x00000000); /* 0x10F4 */
		RamWrite32A_LC898122AF(mes1ba,
				       0x3CA175C0); /* 0x10F5       LPF150Hz */
		RamWrite32A_LC898122AF(mes1bb, 0x3CA175C0); /* 0x10F6 */
		RamWrite32A_LC898122AF(mes1bc, 0x3F75E8C0); /* 0x10F7 */
		RamWrite32A_LC898122AF(mes1bd, 0x00000000); /* 0x10F8 */
		RamWrite32A_LC898122AF(mes1be, 0x00000000); /* 0x10F9 */

		/* Measure Filter2 Setting */
		RamWrite32A_LC898122AF(mes2aa,
				       0x3CA175C0); /* 0x11F0       LPF150Hz */
		RamWrite32A_LC898122AF(mes2ab, 0x3CA175C0); /* 0x11F1 */
		RamWrite32A_LC898122AF(mes2ac, 0x3F75E8C0); /* 0x11F2 */
		RamWrite32A_LC898122AF(mes2ad, 0x00000000); /* 0x11F3 */
		RamWrite32A_LC898122AF(mes2ae, 0x00000000); /* 0x11F4 */
		RamWrite32A_LC898122AF(mes2ba,
				       0x3CA175C0); /* 0x11F5       LPF150Hz */
		RamWrite32A_LC898122AF(mes2bb, 0x3CA175C0); /* 0x11F6 */
		RamWrite32A_LC898122AF(mes2bc, 0x3F75E8C0); /* 0x11F7 */
		RamWrite32A_LC898122AF(mes2bd, 0x00000000); /* 0x11F8 */
		RamWrite32A_LC898122AF(mes2be, 0x00000000); /* 0x11F9 */
	}
#endif
}

void SrvCon(unsigned char UcDirSel, unsigned char UcSwcCon)
{
	if (UcSwcCon) {
		if (!UcDirSel) {			      /* X Direction */
			RegWriteA_LC898122AF(WH_EQSWX, 0x03); /* 0x0170 */
			RamWrite32A_LC898122AF(sxggf, 0x00000000); /* 0x10B5 */
		} else {				      /* Y Direction */
			RegWriteA_LC898122AF(WH_EQSWY, 0x03); /* 0x0171 */
			RamWrite32A_LC898122AF(syggf, 0x00000000); /* 0x11B5 */
		}
	} else {
		if (!UcDirSel) {			      /* X Direction */
			RegWriteA_LC898122AF(WH_EQSWX, 0x02); /* 0x0170 */
			RamWrite32A_LC898122AF(SXLMT, 0x00000000); /* 0x1477 */
		} else {				      /* Y Direction */
			RegWriteA_LC898122AF(WH_EQSWY, 0x02); /* 0x0171 */
			RamWrite32A_LC898122AF(SYLMT, 0x00000000); /* 0x14F7 */
		}
	}
}

#ifdef MODULE_CALIBRATION

unsigned char LopGan(unsigned char UcDirSel)
{
	unsigned char UcLpAdjSts;

#ifdef HALLADJ_HW
	UcLpAdjSts = LoopGainAdj(UcDirSel);
#else
	MesFil(LOOPGAIN);

	/* Servo ON */
	SrvCon(X_DIR, ON);
	SrvCon(Y_DIR, ON);

	/* Wait 300ms */
	WitTim_LC898122AF(300);

	/* Loop Gain Adjust Initialize */
	LopIni(UcDirSel);

	/* Loop Gain Adjust */
	UcLpAdjSts = LopAdj(UcDirSel);
#endif
	/* Servo OFF */
	SrvCon(X_DIR, OFF);
	SrvCon(Y_DIR, OFF);

	if (!UcLpAdjSts)
		return EXE_END;

	if (!UcDirSel)
		return EXE_LXADJ;
	else
		return EXE_LYADJ;
}

#ifndef HALLADJ_HW

void LopIni(unsigned char UcDirSel)
{
	/* Loop Gain Value Initialize */
	LopPar(UcDirSel);

	/* Sign Wave Output Setting */
	LopSin(UcDirSel, ON);
}
#endif


void LopPar(unsigned char UcDirSel)
{
	unsigned short UsLopGan;

	RamAccFixMod(ON); /* Fix mode */

	if (!UcDirSel) {
		UsLopGan = SXGAIN_LOP;
		RamWriteA_LC898122AF(sxg, UsLopGan); /* 0x10D3 */
	} else {
		UsLopGan = SYGAIN_LOP;
		RamWriteA_LC898122AF(syg, UsLopGan); /* 0x11D3 */
	}

	RamAccFixMod(OFF); /* Float mode */
}

#ifndef HALLADJ_HW

void LopSin(unsigned char UcDirSel, unsigned char UcSonOff)
{
	unsigned short UsFreqVal;
	unsigned char UcEqSwX, UcEqSwY;

	RegReadA_LC898122AF(WH_EQSWX, &UcEqSwX); /* 0x0170       */
	RegReadA_LC898122AF(WH_EQSWY, &UcEqSwY); /* 0x0171       */

	if (UcSonOff) {

#ifdef USE_EXTCLK_ALL /* 24MHz */
		      /* Freq = CmSinFrq * 11.718kHz / 65536 / 16 */
#ifdef ACTREG_6P5OHM
		/* UsFreqVal       =       0x30EE ;  */ /* 139.9Hz */
		UsFreqVal = 0x29F1;			/* 119.9Hz */
#endif
#ifdef ACTREG_10P2OHM
		UsFreqVal = 0x29F1; /* 119.9Hz */
#endif
#ifdef ACTREG_15OHM
		UsFreqVal = 0x3B6B; /* 169.9Hz */
#endif
#else
/* Freq = CmSinFrq * 23.4375kHz / 65536 / 16 */
#ifdef ACTREG_6P5OHM
		/* UsFreqVal       =       0x1877 ;   */ /* 139.9Hz */
		UsFreqVal = 0x14F8; /* 119.9Hz */
#endif
#ifdef ACTREG_10P2OHM
		UsFreqVal = 0x14F8; /* 119.9Hz */
#endif
#ifdef ACTREG_15OHM
		UsFreqVal = 0x1DB5; /* 169.9Hz */
#endif
#endif

		RegWriteA_LC898122AF(WC_SINFRQ0,
				     (unsigned char)
					     UsFreqVal); /* 0x0181 Freq L */
		RegWriteA_LC898122AF(WC_SINFRQ1,
				     (unsigned char)(UsFreqVal >> 8));

		if (!UcDirSel) {

			UcEqSwX |= 0x10;
			UcEqSwY &= ~EQSINSW;

			RamWrite32A_LC898122AF(sxsin,
					       0x3CA3D70A); /* 0x10D5 -34dB */
		} else {

			UcEqSwX &= ~EQSINSW;
			UcEqSwY |= 0x10;

			RamWrite32A_LC898122AF(sysin,
					       0x3CA3D70A); /* 0x11D5 -34dB */
		}
		RegWriteA_LC898122AF(WC_SINPHSX,
				     0x00); /* 0x0183       X Sine phase */
		RegWriteA_LC898122AF(WC_SINPHSY,
				     0x00); /* 0x0184       Y Sine phase */
		RegWriteA_LC898122AF(WH_EQSWX,
				     UcEqSwX); /* 0x0170       Switch control */
		RegWriteA_LC898122AF(WH_EQSWY,
				     UcEqSwY); /* 0x0171       Switch control */
		RegWriteA_LC898122AF(WC_SINON,
				     0x01); /* 0x0180       Sine wave  */
	} else {
		UcEqSwX &= ~EQSINSW;
		UcEqSwY &= ~EQSINSW;
		RegWriteA_LC898122AF(WC_SINON,
				     0x00); /* 0x0180       Sine wave  */
		if (!UcDirSel)
			RamWrite32A_LC898122AF(sxsin, 0x00000000); /* 0x10D5 */
		else
			RamWrite32A_LC898122AF(sysin, 0x00000000); /* 0x11D5 */

		RegWriteA_LC898122AF(WH_EQSWX,
				     UcEqSwX); /* 0x0170       Switch control */
		RegWriteA_LC898122AF(WH_EQSWY,
				     UcEqSwY); /* 0x0171       Switch control */
	}
}


unsigned char LopAdj(unsigned char UcDirSel)
{
	unsigned char UcAdjSts = FAILURE;
	unsigned short UsRtnVal;
	float SfCmpVal;
	unsigned char UcIdxCnt;
	unsigned char UcIdxCn1;
	unsigned char UcIdxCn2;
	union UnFltVal UnAdcXg1, UnAdcXg2, UnRtnVa;

	float DfGanVal[5];
	float DfTemVal;

	if (!UcDirSel) {
		RegWriteA_LC898122AF(WC_MES1ADD0,
				     (unsigned char)SXGZ); /* 0x0194 */
		RegWriteA_LC898122AF(
			WC_MES1ADD1,
			(unsigned char)((SXGZ >> 8) & 0x0001)); /* 0x0195 */
		RegWriteA_LC898122AF(WC_MES2ADD0,
				     (unsigned char)SXG3Z); /* 0x0196 */
		RegWriteA_LC898122AF(
			WC_MES2ADD1,
			(unsigned char)((SXG3Z >> 8) & 0x0001)); /* 0x0197 */
	} else {
		RegWriteA_LC898122AF(WC_MES1ADD0,
				     (unsigned char)SYGZ); /* 0x0194 */
		RegWriteA_LC898122AF(
			WC_MES1ADD1,
			(unsigned char)((SYGZ >> 8) & 0x0001)); /* 0x0195 */
		RegWriteA_LC898122AF(WC_MES2ADD0,
				     (unsigned char)SYG3Z); /* 0x0196 */
		RegWriteA_LC898122AF(
			WC_MES2ADD1,
			(unsigned char)((SYG3Z >> 8) & 0x0001)); /* 0x0197 */
	}

	/* 5 Times Average Value Calculation */
	for (UcIdxCnt = 0; UcIdxCnt < 5; UcIdxCnt++) {
		LopMes(); /* Loop Gain Mesurement Start */

		UnAdcXg1.UlLngVal = GinMes(MES_XG1); /* LXG1 Measure */
		UnAdcXg2.UlLngVal = GinMes(MES_XG2); /* LXG2 Measure */

		SfCmpVal = UnAdcXg2.SfFltVal /
			   UnAdcXg1.SfFltVal; /* Compare Coefficient Value */

		if (!UcDirSel)
			RamRead32A_LC898122AF(sxg,
					      &UnRtnVa.UlLngVal); /* 0x10D3 */
		else
			RamRead32A_LC898122AF(syg,
					      &UnRtnVa.UlLngVal); /* 0x11D3 */

		UnRtnVa.SfFltVal = UnRtnVa.SfFltVal * SfCmpVal;

		DfGanVal[UcIdxCnt] = UnRtnVa.SfFltVal;
	}

	for (UcIdxCn1 = 0; UcIdxCn1 < 4; UcIdxCn1++) {
		for (UcIdxCn2 = UcIdxCn1 + 1; UcIdxCn2 < 5; UcIdxCn2++) {
			if (DfGanVal[UcIdxCn1] > DfGanVal[UcIdxCn2]) {
				DfTemVal = DfGanVal[UcIdxCn1];
				DfGanVal[UcIdxCn1] = DfGanVal[UcIdxCn2];
				DfGanVal[UcIdxCn2] = DfTemVal;
			}
		}
	}

	UnRtnVa.SfFltVal = (DfGanVal[1] + DfGanVal[2] + DfGanVal[3]) / 3;

	LopSin(UcDirSel, OFF);

	if (UnRtnVa.UlLngVal < 0x3F800000) /* Adjust Error */
		UcAdjSts = SUCCESS;	/* Status OK */

	if (UcAdjSts) {
		if (!UcDirSel) {
			RamWrite32A_LC898122AF(sxg, 0x3F800000); /* 0x10D3 */
			StAdjPar.StLopGan.UsLxgVal = 0x7FFF;
			StAdjPar.StLopGan.UsLxgSts = 0x0000;
		} else {
			RamWrite32A_LC898122AF(syg, 0x3F800000); /* 0x11D3 */
			StAdjPar.StLopGan.UsLygVal = 0x7FFF;
			StAdjPar.StLopGan.UsLygSts = 0x0000;
		}
	} else {
		if (!UcDirSel) {
			RamWrite32A_LC898122AF(sxg,
					       UnRtnVa.UlLngVal); /* 0x10D3 */
			RamAccFixMod(ON);			  /* Fix mode */
			RamReadA_LC898122AF(sxg, &UsRtnVal);      /* 0x10D3 */
			StAdjPar.StLopGan.UsLxgVal = UsRtnVal;
			StAdjPar.StLopGan.UsLxgSts = 0xFFFF;
		} else {
			RamWrite32A_LC898122AF(syg,
					       UnRtnVa.UlLngVal); /* 0x11D3 */
			RamAccFixMod(ON);			  /* Fix mode */
			RamReadA_LC898122AF(syg, &UsRtnVal);      /* 0x11D3 */
			StAdjPar.StLopGan.UsLygVal = UsRtnVal;
			StAdjPar.StLopGan.UsLygSts = 0xFFFF;
		}
		RamAccFixMod(OFF); /* Float mode */
	}
	return UcAdjSts;
}

void LopMes(void)
{
	ClrGyr(0x1000, CLR_FRAM1); /* Measure Filter RAM Clear */
	RamWrite32A_LC898122AF(MSABS1AV, 0x00000000); /* 0x1041       Clear */
	RamWrite32A_LC898122AF(MSABS2AV, 0x00000000); /* 0x1141       Clear */
	RegWriteA_LC898122AF(WC_MESLOOP1, 0x04);      /* 0x0193 */
	RegWriteA_LC898122AF(WC_MESLOOP0,
			     0x00); /* 0x0192       1024 Times Measure */
	RamWrite32A_LC898122AF(msmean,
			       0x3A800000); /* 0x1230       1/CmMesLoop[15:0] */
	RegWriteA_LC898122AF(WC_MESABS, 0x01);  /* 0x0198       ABS */
	RegWriteA_LC898122AF(WC_MESWAIT, 0x00); /* 0x0199       0 cross wait */
	BsyWit(WC_MESMODE, 0x01); /* 0x0190       Sin Wave Measure */
}


unsigned long GinMes(unsigned char UcXg1Xg2)
{
	unsigned long UlMesVal;

	if (!UcXg1Xg2)
		RamRead32A_LC898122AF(MSABS1AV, &UlMesVal); /* 0x1041 */
	else
		RamRead32A_LC898122AF(MSABS2AV, &UlMesVal); /* 0x1141 */

	return UlMesVal;
}

#endif
#endif


#define LIMITH 0x0FA0
#define LIMITL 0xF060
#define INITVAL 0x0000
unsigned char TneGvc(void)
{
	unsigned char UcRsltSts;

	/* A/D Offset Clear */
	RegWriteA_LC898122AF(
		IZAH, (unsigned char)(INITVAL >> 8));
	RegWriteA_LC898122AF(
		IZAL,
		(unsigned char)
			INITVAL);
	RegWriteA_LC898122AF(
		IZBH, (unsigned char)(INITVAL >> 8));
	RegWriteA_LC898122AF(
		IZBL,
		(unsigned char)
			INITVAL);

	MesFil(THROUGH);
	/* //////// */
	/* X */
	/* //////// */
	RegWriteA_LC898122AF(WC_MES1ADD0, 0x00); /* 0x0194 */
	RegWriteA_LC898122AF(WC_MES1ADD1, 0x00); /* 0x0195 */
	ClrGyr(0x1000, CLR_FRAM1);
	StAdjPar.StGvcOff.UsGxoVal = (unsigned short)GenMes(AD2Z, 0);

	RegWriteA_LC898122AF(IZAH,
			     (unsigned char)(StAdjPar.StGvcOff.UsGxoVal >> 8));

	RegWriteA_LC898122AF(IZAL, (unsigned char)(StAdjPar.StGvcOff.UsGxoVal));

	/* //////// */
	/* Y */
	/* //////// */
	RegWriteA_LC898122AF(WC_MES1ADD0, 0x00); /* 0x0194 */
	RegWriteA_LC898122AF(WC_MES1ADD1, 0x00); /* 0x0195 */
	ClrGyr(0x1000, CLR_FRAM1);		 /* Measure Filter RAM Clear */
	StAdjPar.StGvcOff.UsGyoVal = (unsigned short)GenMes(AD3Z, 0);
	/* 64回の平均値測定     GYRMON2(0x1111) <- GYADZ(0x14CA) */
	RegWriteA_LC898122AF(IZBH,
			     (unsigned char)(StAdjPar.StGvcOff.UsGyoVal >> 8));
	/* 0x02A2               Set Offset High byte */
	RegWriteA_LC898122AF(IZBL, (unsigned char)(StAdjPar.StGvcOff.UsGyoVal));
	/* 0x02A3               Set Offset Low byte */

	UcRsltSts = EXE_END; /* Clear Status */

	StAdjPar.StGvcOff.UsGxoSts = 0xFFFF;
	if (((short)StAdjPar.StGvcOff.UsGxoVal < (short)LIMITL) ||
	    ((short)StAdjPar.StGvcOff.UsGxoVal > (short)LIMITH)) {
		UcRsltSts |= EXE_GXADJ;
		StAdjPar.StGvcOff.UsGxoSts = 0x0000;
	}

	StAdjPar.StGvcOff.UsGyoSts = 0xFFFF;
	if (((short)StAdjPar.StGvcOff.UsGyoVal < (short)LIMITL) ||
	    ((short)StAdjPar.StGvcOff.UsGyoVal > (short)LIMITH)) {
		UcRsltSts |= EXE_GYADJ;
		StAdjPar.StGvcOff.UsGyoSts = 0x0000;
	}
	return UcRsltSts;
}

unsigned char RtnCen(unsigned char UcCmdPar)
{
	unsigned char UcCmdSts;

	UcCmdSts = EXE_END;

	GyrCon(OFF); /* Gyro OFF */

	if (!UcCmdPar) { /* X,Y Centering */

		StbOnn(); /* Slope Mode */

	} else if (UcCmdPar == 0x01) { /* X Centering Only */

		SrvCon(X_DIR, ON); /* X only Servo ON */
		SrvCon(Y_DIR, OFF);
	} else if (UcCmdPar == 0x02) { /* Y Centering Only */

		SrvCon(X_DIR, OFF); /* Y only Servo ON */
		SrvCon(Y_DIR, ON);
	}

	return UcCmdSts;
}

void GyrCon(unsigned char UcGyrCon)
{
	/* Return HPF Setting */
	RegWriteA_LC898122AF(WG_SHTON, 0x00); /* 0x0107 */

	if (UcGyrCon == ON) { /* Gyro ON */

#ifdef GAIN_CONT
	/* Gain3 Register */
	/* AutoGainControlSw( ON ) ;  */ /* Auto Gain Control Mode ON */
#endif
		ClrGyr(0x000E, CLR_FRAM1); /* Gyro Delay RAM Clear */

		RamWrite32A_LC898122AF(sxggf, 0x3F800000); /* 0x10B5 */
		RamWrite32A_LC898122AF(syggf, 0x3F800000); /* 0x11B5 */

	} else if (UcGyrCon == SPC) { /* Gyro ON for LINE */

#ifdef GAIN_CONT
	/* Gain3 Register */
	/* AutoGainControlSw( ON ) ;  */ /* Auto Gain Control Mode ON */
#endif

		RamWrite32A_LC898122AF(sxggf, 0x3F800000); /* 0x10B5 */
		RamWrite32A_LC898122AF(syggf, 0x3F800000); /* 0x11B5 */

	} else { /* Gyro OFF */

		RamWrite32A_LC898122AF(sxggf, 0x00000000); /* 0x10B5 */
		RamWrite32A_LC898122AF(syggf, 0x00000000); /* 0x11B5 */

#ifdef GAIN_CONT
		/* Gain3 Register */
		/* AutoGainControlSw( OFF ); */
#endif
	}
}

void OisEna(void)
{
	/* Servo ON */
	SrvCon(X_DIR, ON);
	SrvCon(Y_DIR, ON);

	GyrCon(ON);
}


void OisEnaLin(void)
{
	/* Servo ON */
	SrvCon(X_DIR, ON);
	SrvCon(Y_DIR, ON);

	GyrCon(SPC);
}


void TimPro(void)
{
#ifdef MODULE_CALIBRATION
	if (UcOscAdjFlg) {
		if (UcOscAdjFlg == MEASSTR) {
			RegWriteA_LC898122AF(
				OSCCNTEN,
				0x01); /* 0x0258       OSC Cnt enable */
			UcOscAdjFlg = MEASCNT;
		} else if (UcOscAdjFlg == MEASCNT) {
			RegWriteA_LC898122AF(
				OSCCNTEN,
				0x00); /* 0x0258       OSC Cnt disable */
			UcOscAdjFlg = MEASFIX;
		}
	}
#endif
}


void S2cPro(unsigned char uc_mode)
{
	if (uc_mode == 1) {
#ifdef H1COEF_CHANGER
		SetH1cMod(S2MODE); /* cancel Lvl change */
#endif
		/* HPF→Through Setting */
		RegWriteA_LC898122AF(WG_SHTON, 0x11);    /* 0x0107 */
		RamWrite32A_LC898122AF(gxh1c, DIFIL_S2); /* 0x1012 */
		RamWrite32A_LC898122AF(gyh1c, DIFIL_S2); /* 0x1112 */
	} else {
		RamWrite32A_LC898122AF(gxh1c, UlH1Coefval); /* 0x1012 */
		RamWrite32A_LC898122AF(gyh1c, UlH1Coefval); /* 0x1112 */
		/* HPF→Through Setting */
		RegWriteA_LC898122AF(WG_SHTON, 0x00); /* 0x0107 */

#ifdef H1COEF_CHANGER
		SetH1cMod(UcH1LvlMod); /* Re-setting */
#endif
	}
}

short GenMes(unsigned short UsRamAdd, unsigned char UcMesMod)
{
	short SsMesRlt;

	RegWriteA_LC898122AF(WC_MES1ADD0, (unsigned char)UsRamAdd); /* 0x0194 */
	RegWriteA_LC898122AF(WC_MES1ADD1, (unsigned char)((UsRamAdd >> 8) &
							  0x0001)); /* 0x0195 */
	RamWrite32A_LC898122AF(MSABS1AV, 0x00000000); /* 0x1041       Clear */

	if (!UcMesMod) {
		RegWriteA_LC898122AF(WC_MESLOOP1, 0x04); /* 0x0193 */
		RegWriteA_LC898122AF(
			WC_MESLOOP0,
			0x00); /* 0x0192       1024 Times Measure */
		RamWrite32A_LC898122AF(
			msmean,
			0x3A7FFFF7); /* 0x1230       1/CmMesLoop[15:0] */
	} else {
		RegWriteA_LC898122AF(WC_MESLOOP1, 0x01); /* 0x0193 */
		RegWriteA_LC898122AF(WC_MESLOOP0,
				     0x00); /* 0x0192       1 Times Measure */
		RamWrite32A_LC898122AF(
			msmean,
			0x3F800000); /* 0x1230       1/CmMesLoop[15:0] */
	}

	RegWriteA_LC898122AF(WC_MESABS, 0x00); /* 0x0198       none ABS */
	BsyWit(WC_MESMODE, 0x01);	      /* 0x0190       normal Measure */

	RamAccFixMod(ON); /* Fix mode */

	RamReadA_LC898122AF(MSABS1AV, (unsigned short *)&SsMesRlt); /* 0x1041 */

	RamAccFixMod(OFF); /* Float mode */

	return SsMesRlt;
}

#ifdef USE_EXTCLK_ALL /* 24MHz */
		      /********* Parameter Setting *********/
/* Servo Sampling Clock         =       11.71875kHz */
/* Freq                                         =       CmSinFreq*Fs/65536/16 */
/* 05 00 XX MM                          XX:Freq MM:Sin or Circle */
const unsigned short CucFreqVal[17] = {
	0xFFFF, /* 0:  Stop */
	0x0059, /* 1: 0.994653Hz */
	0x00B2, /* 2: 1.989305Hz */
	0x010C, /* 3: 2.995133Hz */
	0x0165, /* 4: 3.989786Hz */
	0x01BF, /* 5: 4.995614Hz */
	0x0218, /* 6: 5.990267Hz */
	0x0272, /* 7: 6.996095Hz */
	0x02CB, /* 8: 7.990748Hz */
	0x0325, /* 9: 8.996576Hz */
	0x037E, /* A: 9.991229Hz */
	0x03D8, /* B: 10.99706Hz */
	0x0431, /* C: 11.99171Hz */
	0x048B, /* D: 12.99754Hz */
	0x04E4, /* E: 13.99219Hz */
	0x053E, /* F: 14.99802Hz */
	0x0597  /* 10: 15.99267Hz */
};
#else
/********* Parameter Setting *********/
/* Servo Sampling Clock         =       23.4375kHz */
/* Freq                                         =       CmSinFreq*Fs/65536/16 */
/* 05 00 XX MM                          XX:Freq MM:Sin or Circle */
const unsigned short CucFreqVal[17] = {
	0xFFFF, /* 0:  Stop */
	0x002C, /* 1: 0.983477Hz */
	0x0059, /* 2: 1.989305Hz */
	0x0086, /* 3: 2.995133Hz */
	0x00B2, /* 4: 3.97861Hz */
	0x00DF, /* 5: 4.984438Hz */
	0x010C, /* 6: 5.990267Hz */
	0x0139, /* 7: 6.996095Hz */
	0x0165, /* 8: 7.979572Hz */
	0x0192, /* 9: 8.9854Hz */
	0x01BF, /* A: 9.991229Hz */
	0x01EC, /* B: 10.99706Hz */
	0x0218, /* C: 11.98053Hz */
	0x0245, /* D: 12.98636Hz */
	0x0272, /* E: 13.99219Hz */
	0x029F, /* F: 14.99802Hz */
	0x02CB  /* 10: 15.9815Hz */
};
#endif

#define USE_SINLPF

void SetSinWavePara(unsigned char UcTableVal, unsigned char UcMethodVal)
{
	unsigned short UsFreqDat;
	unsigned char UcEqSwX, UcEqSwY;

	if (UcTableVal > 0x10)
		UcTableVal = 0x10; /* Limit */
	UsFreqDat = CucFreqVal[UcTableVal];

	if (UcMethodVal == SINEWAVE) {
		RegWriteA_LC898122AF(WC_SINPHSX, 0x00); /* 0x0183       */
		RegWriteA_LC898122AF(WC_SINPHSY, 0x00); /* 0x0184       */
	} else if (UcMethodVal == CIRCWAVE) {
		RegWriteA_LC898122AF(WC_SINPHSX, 0x00); /* 0x0183       */
		RegWriteA_LC898122AF(WC_SINPHSY, 0x20); /* 0x0184       */
	} else {
		RegWriteA_LC898122AF(WC_SINPHSX, 0x00); /* 0x0183       */
		RegWriteA_LC898122AF(WC_SINPHSY, 0x00); /* 0x0184       */
	}

#ifdef USE_SINLPF
	if ((UcMethodVal != XHALWAVE) && (UcMethodVal != YHALWAVE))
		MesFil(NOISE); /* LPF */
#endif

	if (UsFreqDat == 0xFFFF) { /* Sine波?止 */

		RegReadA_LC898122AF(WH_EQSWX, &UcEqSwX); /* 0x0170       */
		RegReadA_LC898122AF(WH_EQSWY, &UcEqSwY); /* 0x0171       */
		UcEqSwX &= ~EQSINSW;
		UcEqSwY &= ~EQSINSW;
		RegWriteA_LC898122AF(WH_EQSWX, UcEqSwX); /* 0x0170       */
		RegWriteA_LC898122AF(WH_EQSWY, UcEqSwY); /* 0x0171       */

#ifdef USE_SINLPF
		if ((UcMethodVal != XHALWAVE) && (UcMethodVal != YHALWAVE)) {
			RegWriteA_LC898122AF(
				WC_DPON, 0x00);
			RegWriteA_LC898122AF(
				WC_DPO1ADD0,
				0x00); /* 0x01B8       output initial */
			RegWriteA_LC898122AF(
				WC_DPO1ADD1,
				0x00); /* 0x01B9       output initial */
			RegWriteA_LC898122AF(
				WC_DPO2ADD0,
				0x00); /* 0x01BA       output initial */
			RegWriteA_LC898122AF(
				WC_DPO2ADD1,
				0x00); /* 0x01BB       output initial */
			RegWriteA_LC898122AF(
				WC_DPI1ADD0,
				0x00); /* 0x01B0       input initial */
			RegWriteA_LC898122AF(
				WC_DPI1ADD1,
				0x00); /* 0x01B1       input initial */
			RegWriteA_LC898122AF(
				WC_DPI2ADD0,
				0x00); /* 0x01B2       input initial */
			RegWriteA_LC898122AF(
				WC_DPI2ADD1,
				0x00); /* 0x01B3       input initial */

			/* Ram Access */
			RamAccFixMod(ON); /* Fix mode */

			RamWriteA_LC898122AF(
				SXOFFZ1,
				UsCntXof); /* 0x1461       set optical value */
			RamWriteA_LC898122AF(
				SYOFFZ1,
				UsCntYof); /* 0x14E1       set optical value */

			/* Ram Access */
			RamAccFixMod(OFF); /* Float mode */

			RegWriteA_LC898122AF(WC_MES1ADD0, 0x00); /* 0x0194 */
			RegWriteA_LC898122AF(WC_MES1ADD1, 0x00); /* 0x0195 */
			RegWriteA_LC898122AF(WC_MES2ADD0, 0x00); /* 0x0196 */
			RegWriteA_LC898122AF(WC_MES2ADD1, 0x00); /* 0x0197 */
		}
#endif
		RegWriteA_LC898122AF(WC_SINON,
				     0x00); /* 0x0180       Sine wave  */

	} else {

		RegReadA_LC898122AF(WH_EQSWX, &UcEqSwX); /* 0x0170       */
		RegReadA_LC898122AF(WH_EQSWY, &UcEqSwY); /* 0x0171       */

		if ((UcMethodVal != XHALWAVE) && (UcMethodVal != YHALWAVE)) {
#ifdef USE_SINLPF
			RegWriteA_LC898122AF(WC_DPI1ADD0,
					     (unsigned char)MES1BZ2);
			/* 0x01B0       input Meas-Fil */
			RegWriteA_LC898122AF(
				WC_DPI1ADD1,
				(unsigned char)((MES1BZ2 >> 8) & 0x0001));
			/* 0x01B1       input Meas-Fil */
			RegWriteA_LC898122AF(WC_DPI2ADD0,
					     (unsigned char)MES2BZ2);
			/* 0x01B2       input Meas-Fil */
			RegWriteA_LC898122AF(
				WC_DPI2ADD1,
				(unsigned char)((MES2BZ2 >> 8) & 0x0001));
			/* 0x01B3       input Meas-Fil */
			RegWriteA_LC898122AF(WC_DPO1ADD0,
					     (unsigned char)SXOFFZ1);
			/* 0x01B8       output SXOFFZ1 */
			RegWriteA_LC898122AF(
				WC_DPO1ADD1,
				(unsigned char)((SXOFFZ1 >> 8) & 0x0001));
			/* 0x01B9       output SXOFFZ1 */
			RegWriteA_LC898122AF(WC_DPO2ADD0,
					     (unsigned char)SYOFFZ1);
			/* 0x01BA       output SYOFFZ1 */
			RegWriteA_LC898122AF(
				WC_DPO2ADD1,
				(unsigned char)((SYOFFZ1 >> 8) & 0x0001));
			/* 0x01BA       output SYOFFZ1 */

			RegWriteA_LC898122AF(WC_MES1ADD0,
					     (unsigned char)SINXZ); /* 0x0194 */
			RegWriteA_LC898122AF(
				WC_MES1ADD1,
				(unsigned char)((SINXZ >> 8) & 0x0001));
			RegWriteA_LC898122AF(WC_MES2ADD0,
					     (unsigned char)SINYZ); /* 0x0196 */
			RegWriteA_LC898122AF(
				WC_MES2ADD1,
				(unsigned char)((SINYZ >> 8) & 0x0001));

			RegWriteA_LC898122AF(
				WC_DPON,
				0x03); /* 0x0105       Data pass[1:0] on */

			UcEqSwX &= ~EQSINSW;
			UcEqSwY &= ~EQSINSW;
#else
			UcEqSwX |= 0x08;
			UcEqSwY |= 0x08;
#endif

		} else {
			if (UcMethodVal == XHALWAVE) {
				UcEqSwX = 0x22; /* SW[5] */
						/* UcEqSwY = 0x03 ; */
			} else {
				/* UcEqSwX = 0x03 ; */
				UcEqSwY = 0x22; /* SW[5] */
			}
		}

		RegWriteA_LC898122AF(WC_SINFRQ0, (unsigned char)UsFreqDat);
		RegWriteA_LC898122AF(WC_SINFRQ1,
				     (unsigned char)(UsFreqDat >> 8));
		RegWriteA_LC898122AF(WC_MESSINMODE,
				     0x00); /* 0x0191       Sine 0 cross  */

		RegWriteA_LC898122AF(WH_EQSWX, UcEqSwX); /* 0x0170       */
		RegWriteA_LC898122AF(WH_EQSWY, UcEqSwY); /* 0x0171       */

		RegWriteA_LC898122AF(WC_SINON,
				     0x01); /* 0x0180       Sine wave  */
	}
}

#ifdef STANDBY_MODE
void SetStandby(unsigned char UcContMode)
{
	unsigned char UcStbb0, UcClkon;

	switch (UcContMode) {
	case STB1_ON:

#ifdef AF_PWMMODE
#else
		RegWriteA_LC898122AF(DRVFCAF, 0x00);

#endif
		RegWriteA_LC898122AF(STBB0, 0x00);

		RegWriteA_LC898122AF(STBB1, 0x00);

		RegWriteA_LC898122AF(
			PWMA, 0x00); /* 0x0010               PWM Standby */
		RegWriteA_LC898122AF(
			PWMAAF, 0x00); /* 0x0090               AF PWM Standby */
		RegWriteA_LC898122AF(
			CVA, 0x00); /* 0x0020       LINEAR PWM mode standby */
		DrvSw(OFF);	 /* Driver OFF */
		AfDrvSw(OFF);       /* AF Driver OFF */
#ifdef MONITOR_OFF
#else
		RegWriteA_LC898122AF(PWMMONA,
				     0x00); /* 0x0030       Monitor Standby */
#endif

		SelectGySleep(ON); /* Gyro Sleep */
		break;
	case STB1_OFF:
		SelectGySleep(OFF); /* Gyro Wake Up */

		RegWriteA_LC898122AF(PWMMONA,
				     0x80); /* 0x0030       Monitor Active  */
		DrvSw(ON);		    /* Driver Mode setting */
		AfDrvSw(ON);		    /* AF Driver Mode setting */
		RegWriteA_LC898122AF(
			CVA, 0xC0); /* 0x0020       Linear PWM mode enable */
		RegWriteA_LC898122AF(
			PWMAAF, 0x00); /* 0x0090               AF PWM Standby */
		RegWriteA_LC898122AF(PWMA, 0xC0); /* 0x0010       PWM enable */
		RegWriteA_LC898122AF(STBB1, 0x05);

		RegWriteA_LC898122AF(STBB0, 0xDF);

		break;
	case STB2_ON:
#ifdef AF_PWMMODE
#else
		RegWriteA_LC898122AF(DRVFCAF, 0x00);

#endif
		RegWriteA_LC898122AF(STBB0, 0x00);

		RegWriteA_LC898122AF(STBB1, 0x00);

		RegWriteA_LC898122AF(
			PWMA, 0x00); /* 0x0010               PWM Standby */
		RegWriteA_LC898122AF(
			PWMAAF, 0x00); /* 0x0090               AF PWM Standby */
		RegWriteA_LC898122AF(
			CVA, 0x00); /* 0x0020       LINEAR PWM mode standby */
		DrvSw(OFF);	 /* Drvier Block Ena=0 */
		AfDrvSw(OFF);       /* AF Drvier Block Ena=0 */
#ifdef MONITOR_OFF
#else
		RegWriteA_LC898122AF(PWMMONA,
				     0x00); /* 0x0030       Monitor Standby */
#endif

		SelectGySleep(ON); /* Gyro Sleep */
		RegWriteA_LC898122AF(CLKON,
				     0x00);
		break;
	case STB2_OFF:
		RegWriteA_LC898122AF(CLKON, 0x1F);

		SelectGySleep(OFF); /* Gyro Wake Up */

		RegWriteA_LC898122AF(PWMMONA,
				     0x80); /* 0x0030       Monitor Active  */
		DrvSw(ON);		    /* Driver Mode setting */
		AfDrvSw(ON);		    /* AF Driver Mode setting */
		RegWriteA_LC898122AF(
			CVA, 0xC0); /* 0x0020       Linear PWM mode enable */
		RegWriteA_LC898122AF(
			PWMAAF, 0x00); /* 0x0090               AF PWM Standby */
		RegWriteA_LC898122AF(PWMA, 0xC0); /* 0x0010       PWM enable */
		RegWriteA_LC898122AF(STBB1, 0x05);

		RegWriteA_LC898122AF(STBB0, 0xDF);

		break;
	case STB3_ON:
#ifdef AF_PWMMODE
#else
		RegWriteA_LC898122AF(DRVFCAF, 0x00);
/* 0x0081       Drv.MODEAF=0,Drv.ENAAF=0,MODE-0 */
#endif
		RegWriteA_LC898122AF(STBB0, 0x00);

		RegWriteA_LC898122AF(STBB1, 0x00);

		RegWriteA_LC898122AF(
			PWMA, 0x00); /* 0x0010               PWM Standby */
		RegWriteA_LC898122AF(
			PWMAAF, 0x00); /* 0x0090               AF PWM Standby */
		RegWriteA_LC898122AF(
			CVA, 0x00); /* 0x0020       LINEAR PWM mode standby */
		DrvSw(OFF);	 /* Drvier Block Ena=0 */
		AfDrvSw(OFF);       /* AF Drvier Block Ena=0 */
#ifdef MONITOR_OFF
#else
		RegWriteA_LC898122AF(PWMMONA,
				     0x00); /* 0x0030       Monitor Standby */
#endif

		SelectGySleep(ON); /* Gyro Sleep */
		RegWriteA_LC898122AF(CLKON,
				     0x00);
		RegWriteA_LC898122AF(
			I2CSEL,
			0x01); /* 0x0248       I2C Noise Cancel circuit OFF */
		RegWriteA_LC898122AF(
			OSCSTOP,
			0x02); /* 0x0256       Source Clock Input OFF */
		break;
	case STB3_OFF:
		RegWriteA_LC898122AF(
			OSCSTOP, 0x00); /* 0x0256       Source Clock Input ON */
		RegWriteA_LC898122AF(
			I2CSEL,
			0x00); /* 0x0248       I2C Noise Cancel circuit ON */
		RegWriteA_LC898122AF(CLKON, 0x1F);

		SelectGySleep(OFF); /* Gyro Wake Up */

		RegWriteA_LC898122AF(PWMMONA, 0x80);
		/* 0x0030       Monitor Active  */
		DrvSw(ON);   /* Driver Mode setting */
		AfDrvSw(ON); /* AF Driver Mode setting */
		RegWriteA_LC898122AF(CVA, 0xC0);
		/* 0x0020       Linear PWM mode enable */
		RegWriteA_LC898122AF(PWMAAF, 0x00);
		/* 0x0090               AF PWM Standby */
		RegWriteA_LC898122AF(PWMA, 0xC0);
		/* 0x0010       PWM enable */
		RegWriteA_LC898122AF(STBB1, 0x05);

		RegWriteA_LC898122AF(STBB0, 0xDF);

		break;

	case STB4_ON:
#ifdef AF_PWMMODE
#else
		RegWriteA_LC898122AF(DRVFCAF, 0x00);

#endif
		RegWriteA_LC898122AF(STBB0, 0x00);

		RegWriteA_LC898122AF(STBB1, 0x00);

		RegWriteA_LC898122AF(
			PWMA, 0x00); /* 0x0010               PWM Standby */
		RegWriteA_LC898122AF(
			PWMAAF, 0x00); /* 0x0090               AF PWM Standby */
		RegWriteA_LC898122AF(
			CVA, 0x00); /* 0x0020       LINEAR PWM mode standby */
		DrvSw(OFF);	 /* Drvier Block Ena=0 */
		AfDrvSw(OFF);       /* AF Drvier Block Ena=0 */
#ifdef MONITOR_OFF
#else
		RegWriteA_LC898122AF(PWMMONA,
				     0x00); /* 0x0030       Monitor Standby */
#endif

		GyOutSignalCont(); /* Gyro Continuos mode */
		RegWriteA_LC898122AF(CLKON, 0x04);
		/* 0x020B       Servo & PWM Clock OFF + D-Gyro I/F ON   */
		break;
	case STB4_OFF:
		RegWriteA_LC898122AF(CLKON, 0x1F);

		SelectGySleep(OFF); /* Gyro OIS mode */

		RegWriteA_LC898122AF(PWMMONA,
				     0x80); /* 0x0030       Monitor Active  */
		DrvSw(ON);		    /* Driver Mode setting */
		AfDrvSw(ON);		    /* AF Driver Mode setting */
		RegWriteA_LC898122AF(
			CVA, 0xC0); /* 0x0020       Linear PWM mode enable */
		RegWriteA_LC898122AF(
			PWMAAF, 0x00); /* 0x0090               AF PWM Standby */
		RegWriteA_LC898122AF(PWMA, 0xC0); /* 0x0010       PWM enable */
		RegWriteA_LC898122AF(STBB1, 0x05);

		RegWriteA_LC898122AF(STBB0, 0xDF);

		break;

	/************** special mode ************/
	case STB2_OISON:
		RegReadA_LC898122AF(STBB0, &UcStbb0);

		UcStbb0 &= 0x80;
		RegWriteA_LC898122AF(STBB0, UcStbb0);

		RegWriteA_LC898122AF(
			PWMA, 0x00); /* 0x0010               PWM Standby */
		RegWriteA_LC898122AF(
			CVA, 0x00); /* 0x0020       LINEAR PWM mode standby */
		DrvSw(OFF);	 /* Drvier Block Ena=0 */
#ifdef MONITOR_OFF
#else
		RegWriteA_LC898122AF(PWMMONA,
				     0x00);
#endif

		SelectGySleep(ON); /* Gyro Sleep */
		RegReadA_LC898122AF(CLKON, &UcClkon);

		UcClkon &= 0x1A;
		RegWriteA_LC898122AF(CLKON, UcClkon);

		break;
	case STB2_OISOFF:
		RegReadA_LC898122AF(CLKON, &UcClkon);
		/* 0x020B       PWM Clock OFF + D-Gyro I/F ON  */
		UcClkon |= 0x05;
		RegWriteA_LC898122AF(CLKON, UcClkon);

		SelectGySleep(OFF); /* Gyro Wake Up */

		RegWriteA_LC898122AF(PWMMONA, 0x80);
		/* 0x0030       Monitor Active  */
		DrvSw(ON);
		/* Driver Mode setting */
		RegWriteA_LC898122AF(CVA, 0xC0);
		/* 0x0020       Linear PWM mode enable */
		RegWriteA_LC898122AF(PWMA, 0xC0);
		/* 0x0010       PWM enable */
		RegReadA_LC898122AF(STBB0, &UcStbb0);

		UcStbb0 |= 0x5F;
		RegWriteA_LC898122AF(STBB0, UcStbb0);

		break;

	case STB2_AFON:
#ifdef AF_PWMMODE
#else
		RegWriteA_LC898122AF(DRVFCAF, 0x00);
/* 0x0081       Drv.MODEAF=0,Drv.ENAAF=0,MODE-0 */
#endif
		RegReadA_LC898122AF(STBB0, &UcStbb0);

		UcStbb0 &= 0x7F;
		RegWriteA_LC898122AF(STBB0, UcStbb0);

		RegWriteA_LC898122AF(STBB1, 0x00);

		RegWriteA_LC898122AF(PWMAAF, 0x00);
		/* 0x0090               AF PWM Standby */
		AfDrvSw(OFF); /* AF Drvier Block Ena=0 */
#ifdef MONITOR_OFF
#else
		RegWriteA_LC898122AF(PWMMONA, 0x00);
/* 0x0030       Monitor Standby */
#endif
		RegReadA_LC898122AF(CLKON, &UcClkon);

		UcClkon &= 0x07;
		RegWriteA_LC898122AF(CLKON, UcClkon);

		break;
	case STB2_AFOFF:
		RegReadA_LC898122AF(CLKON, &UcClkon);
		/* 0x020B       OPAF Clock ON + AFPWM ON  */
		UcClkon |= 0x18;
		RegWriteA_LC898122AF(CLKON, UcClkon);

		AfDrvSw(ON);
		/* AF Driver Mode setting */
		RegWriteA_LC898122AF(PWMAAF, 0x00);
		/* 0x0090               AF PWM Standby */
		RegWriteA_LC898122AF(STBB1, 0x05);

		RegReadA_LC898122AF(STBB0, &UcStbb0);

		UcStbb0 |= 0x80;
		RegWriteA_LC898122AF(STBB0, UcStbb0);

		break;
		/************** special mode ************/
	}
}
#endif

void SetZsp(unsigned char UcZoomStepDat)
{
	unsigned long UlGyrZmx, UlGyrZmy, UlGyrZrx, UlGyrZry;

	/* Zoom Step */
	if (UcZoomStepDat > (ZOOMTBL - 1))
		UcZoomStepDat = (ZOOMTBL - 1); /* 上限をZOOMTBL-1に設定する */

	if (UcZoomStepDat == 0) {       /* initial setting        */
		UlGyrZmx = ClGyxZom[0]; /* Same Wide Coefficient */
		UlGyrZmy = ClGyyZom[0]; /* Same Wide Coefficient */
					/* Initial Rate value = 1 */
	} else {
		UlGyrZmx = ClGyxZom[UcZoomStepDat];
		UlGyrZmy = ClGyyZom[UcZoomStepDat];
	}

	/* Zoom Value Setting */
	RamWrite32A_LC898122AF(gxlens, UlGyrZmx); /* 0x1022 */
	RamWrite32A_LC898122AF(gylens, UlGyrZmy); /* 0x1122 */

	RamRead32A_LC898122AF(gxlens, &UlGyrZrx); /* 0x1022 */
	RamRead32A_LC898122AF(gylens, &UlGyrZry); /* 0x1122 */

	/* Zoom Value Setting Error Check */
	if (UlGyrZmx != UlGyrZrx)
		RamWrite32A_LC898122AF(gxlens, UlGyrZmx); /* 0x1022 */

	if (UlGyrZmy != UlGyrZry)
		RamWrite32A_LC898122AF(gylens, UlGyrZmy); /* 0x1122 */
}

void StbOnn(void)
{
	unsigned char UcRegValx, UcRegValy; /* Registor value */
	unsigned char UcRegIni;

	RegReadA_LC898122AF(WH_EQSWX, &UcRegValx); /* 0x0170 */
	RegReadA_LC898122AF(WH_EQSWY, &UcRegValy); /* 0x0171 */

	if (((UcRegValx & 0x01) != 0x01) && ((UcRegValy & 0x01) != 0x01)) {

		RegWriteA_LC898122AF(WH_SMTSRVON, 0x01);

		SrvCon(X_DIR, ON);
		SrvCon(Y_DIR, ON);

		UcRegIni = 0x11;
		while ((UcRegIni & 0x77) != 0x66)
			RegReadA_LC898122AF(RH_SMTSRVSTT, &UcRegIni);

		RegWriteA_LC898122AF(WH_SMTSRVON, 0x00);

	} else {
		SrvCon(X_DIR, ON);
		SrvCon(Y_DIR, ON);
	}
}


void StbOnnN(unsigned char UcStbY, unsigned char UcStbX)
{
	unsigned char UcRegIni;
	unsigned char UcSttMsk = 0;

	RegWriteA_LC898122AF(WH_SMTSRVON,
			     0x01); /* 0x017C               Smooth Servo ON */
	if (UcStbX == ON)
		UcSttMsk |= 0x07;
	if (UcStbY == ON)
		UcSttMsk |= 0x70;

	SrvCon(X_DIR, UcStbX);
	SrvCon(Y_DIR, UcStbY);

	UcRegIni = 0x11;
	while ((UcRegIni & UcSttMsk) != (0x66 & UcSttMsk))
		RegReadA_LC898122AF(RH_SMTSRVSTT, &UcRegIni);

	RegWriteA_LC898122AF(WH_SMTSRVON, 0x00);
}

void OptCen(unsigned char UcOptmode, unsigned short UsOptXval,
	    unsigned short UsOptYval)
{
	RamAccFixMod(ON); /* Fix mode */

	switch (UcOptmode) {
	case VAL_SET:
		RamWriteA_LC898122AF(SXOFFZ1, UsOptXval);
		RamWriteA_LC898122AF(SYOFFZ1, UsOptYval);
		break;
	case VAL_FIX:
		UsCntXof = UsOptXval;
		UsCntYof = UsOptYval;
		RamWriteA_LC898122AF(SXOFFZ1, UsCntXof);
		RamWriteA_LC898122AF(SYOFFZ1, UsCntYof);

		break;
	case VAL_SPC:
		RamReadA_LC898122AF(SXOFFZ1, &UsOptXval);
		RamReadA_LC898122AF(SYOFFZ1, &UsOptYval);
		UsCntXof = UsOptXval;
		UsCntYof = UsOptYval;

		break;
	}

	RamAccFixMod(OFF); /* Float mode */
}

#ifdef MODULE_CALIBRATION

#define RRATETABLE 8
#define CRATETABLE 16
const signed char ScRselRate[RRATETABLE] = {
	-12, /* -12% */
	-9,  /*  -9% */
	-6,  /*  -6% */
	-3,  /*  -3% */
	0,   /*   0% */
	3,   /*   3% */
	7,   /*   7% */
	11   /*  11% */
};

const signed char ScCselRate[CRATETABLE] = {
	-14, /* -14% */
	-12, /* -12% */
	-10, /* -10% */
	-8,  /*  -8% */
	-6,  /*  -6% */
	-4,  /*  -4% */
	-2,  /*  -2% */
	0,   /*   0% */
	0,   /*   0% */
	2,   /*   2% */
	4,   /*   4% */
	6,   /*   6% */
	8,   /*   8% */
	10,  /*  10% */
	12,  /*  12% */
	14   /*  14% */
};

#define TARGET_FREQ 48000.0F /* 48MHz */

#define START_RSEL 0x04 /* Typ */
#define START_CSEL 0x08 /* Typ bit4:OSCPMSEL */
#define MEAS_MAX 32     /* 上限32回 */
/* Measure Status (UcClkJdg) */
#define UNDR_MEAS 0x00
#define FIX_MEAS 0x01
#define JST_FIX 0x03
#define OVR_MEAS 0x80
/* Measure Check Flag (UcMeasFlg) */
#define RSELFX 0x08
#define RSEL1ST 0x01
#define RSEL2ND 0x02
#define CSELFX 0x80
#define CSELPLS 0x10
#define CSELMNS 0x20

unsigned short OscAdj(void)
{
	unsigned char UcMeasFlg;	    /* Measure check flag */
	union UnWrdVal StClkVal;	    /* Measure value */
	unsigned char UcMeasCnt;	    /* Measure counter */
	unsigned char UcOscrsel, UcOsccsel; /* Reg set value */
	unsigned char UcSrvDivBk;	   /* back up value */
	unsigned char UcClkJdg;		    /* State flag */
	float FcalA, FcalB;		    /* calcurate value */
	signed char ScTblRate_Val, ScTblRate_Now, ScTblRate_Tgt;
	float FlRatePbk, FlRateMbk;
	unsigned char UcOsccselP, UcOsccselM; /* Reg set value */
	unsigned short UsResult;

	/* unsigned char   UcOscsetBk ;  */ /* Reg set value */

	UcMeasFlg = 0;	/* Clear Measure check flag */
	UcMeasCnt = 0;	/* Clear Measure counter */
	UcClkJdg = UNDR_MEAS; /* under Measure */
	UcOscrsel = START_RSEL;
	UcOsccsel = START_CSEL;
	/* check */
	/* RegReadA_LC898122AF( OSCSET, &UcOscsetBk ) ;       // 0x0264 */
	/* UcOscrsel = ( UcOscsetBk & 0xE0 ) >> 5 ; */
	/* UcOsccsel = ( UcOscsetBk & 0x1E ) >> 1 ; */
	/**/ RegReadA_LC898122AF(SRVDIV, &UcSrvDivBk); /* 0x0211 */
	RegWriteA_LC898122AF(SRVDIV,
			     0x00); /* 0x0211        SRV Clock = Xtalck */
	RegWriteA_LC898122AF(OSCSET,
			     (UcOscrsel << 5) | (UcOsccsel << 1)); /* 0x0257 */

	while (UcClkJdg == UNDR_MEAS) {
		UcMeasCnt++;	   /* Measure count up */
		UcOscAdjFlg = MEASSTR; /* Start trigger ON */

		while (UcOscAdjFlg != MEASFIX)
			;

		UcOscAdjFlg = 0x00; /* Clear Flag */
		RegReadA_LC898122AF(OSCCK_CNTR0,
				    &StClkVal.StWrdVal.UcLowVal); /* 0x025E */
		RegReadA_LC898122AF(OSCCK_CNTR1,
				    &StClkVal.StWrdVal.UcHigVal); /* 0x025F */

		FcalA = (float)StClkVal.UsWrdVal;
		FcalB = TARGET_FREQ / FcalA;
		FcalB = FcalB - 1.0F;
		FcalB *= 100.0F;

		if (FcalB == 0.0F) {
			UcClkJdg = JST_FIX;		/* Just 36MHz */
			UcMeasFlg |= (CSELFX | RSELFX); /* Fix Flag */
			break;
		}

		/* Rsel check */
		if (!(UcMeasFlg & RSELFX)) {
			if (UcMeasFlg & RSEL1ST)
				UcMeasFlg |= (RSELFX | RSEL2ND);
			else
				UcMeasFlg |= RSEL1ST;
			ScTblRate_Now = ScRselRate[UcOscrsel]; /* 今のRate */
			ScTblRate_Tgt = ScTblRate_Now + (short)FcalB;
			if (ScTblRate_Now > ScTblRate_Tgt) {
				while (1) {
					if (UcOscrsel == 0)
						break;
					UcOscrsel -= 1;
					ScTblRate_Val = ScRselRate[UcOscrsel];
					if (ScTblRate_Tgt >= ScTblRate_Val)
						break;
				}
			} else if (ScTblRate_Now < ScTblRate_Tgt) {
				while (1) {
					if (UcOscrsel == (RRATETABLE - 1))
						break;
					UcOscrsel += 1;
					ScTblRate_Val = ScRselRate[UcOscrsel];
					if (ScTblRate_Tgt <= ScTblRate_Val)
						break;
				}
			} else {
				;
			}
		} else {
			/* Csel check */
			if (FcalB > 0) { /* Plus */
				UcMeasFlg |= CSELPLS;
				FlRatePbk = FcalB;
				UcOsccselP = UcOsccsel;
				if (UcMeasFlg & CSELMNS) {
					UcMeasFlg |= CSELFX;
					UcClkJdg = FIX_MEAS; /* OK */
				} else if (UcOsccsel == (CRATETABLE - 1)) {
					if (UcOscrsel < (RRATETABLE - 1)) {
						UcOscrsel += 1;
						UcOsccsel = START_CSEL;
						UcMeasFlg = 0; /* Clear */
					} else {
						UcClkJdg = OVR_MEAS; /* Over */
					}
				} else {
					UcOsccsel += 1;
				}
			} else { /* Minus */

				UcMeasFlg |= CSELMNS;
				FlRateMbk = (-1) * FcalB;
				UcOsccselM = UcOsccsel;
				if (UcMeasFlg & CSELPLS) {
					UcMeasFlg |= CSELFX;
					UcClkJdg = FIX_MEAS; /* OK */
				} else if (UcOsccsel == 0x00) {
					if (UcOscrsel > 0) {
						UcOscrsel -= 1;
						UcOsccsel = START_CSEL;
						UcMeasFlg = 0; /* Clear */
					} else {
						UcClkJdg = OVR_MEAS; /* Over */
					}
				} else {
					UcOsccsel -= 1;
				}
			}
			if (UcMeasCnt >= MEAS_MAX)
				UcClkJdg = OVR_MEAS; /* Over */
		}
		RegWriteA_LC898122AF(OSCSET,
				     (UcOscrsel << 5) |
					     (UcOsccsel << 1)); /* 0x0257 */
	}

	UsResult = EXE_END;

	if (UcClkJdg == FIX_MEAS) {
		if (FlRatePbk < FlRateMbk)
			UcOsccsel = UcOsccselP;
		else
			UcOsccsel = UcOsccselM;

		RegWriteA_LC898122AF(OSCSET,
				     (UcOscrsel << 5) |
					     (UcOsccsel << 1)); /* 0x0264 */

		/* check */
		/* RegReadA_LC898122AF( OSCSET, &UcOscsetBk ) ;       // 0x0257
		 */
	}
	StAdjPar.UcOscVal = ((UcOscrsel << 5) | (UcOsccsel << 1));

	if (UcClkJdg == OVR_MEAS) {
		UsResult = EXE_OCADJ;
		StAdjPar.UcOscVal = 0x00;
	}
	RegWriteA_LC898122AF(SRVDIV,
			     UcSrvDivBk); /* 0x0211        SRV Clock set */
	return UsResult;
}
#endif

#ifdef HALLADJ_HW

void SetSineWave(unsigned char UcJikuSel, unsigned char UcMeasMode)
{
#ifdef USE_EXTCLK_ALL						/* 24MHz */
	unsigned short UsFRQ[] = {0x30EE, 0x037E};
#else
	unsigned short UsFRQ[] = {0x1877, 0x01BF};
#endif
	unsigned long UlAMP[2][2] = {
		{0x3CA3D70A, 0x3CA3D70A},  /* Loop Gain   { X amp , Y amp } */
		{0x3F800000, 0x3F800000} }; /* Bias/offset { X amp , Y amp } */
	unsigned char UcEqSwX, UcEqSwY;

	UcMeasMode &= 0x01;
	UcJikuSel &= 0x01;

	/* Phase parameter 0deg */
	RegWriteA_LC898122AF(WC_SINPHSX, 0x00); /* 0x0183       */
	RegWriteA_LC898122AF(WC_SINPHSY, 0x00); /* 0x0184       */

	/* wait 0 cross */
	RegWriteA_LC898122AF(WC_MESSINMODE,
			     0x00);		/* 0x0191       Sine 0 cross  */
	RegWriteA_LC898122AF(WC_MESWAIT, 0x00); /* 0x0199       0 cross wait */

	/* Manually Set Amplitude */
	RamWrite32A_LC898122AF(sxsin, UlAMP[UcMeasMode][X_DIR]); /* 0x10D5 */
	RamWrite32A_LC898122AF(sysin, UlAMP[UcMeasMode][Y_DIR]); /* 0x11D5 */

	/* Freq */
	RegWriteA_LC898122AF(WC_SINFRQ0,
			     (unsigned char)
				     UsFRQ[UcMeasMode]); /* 0x0181 Freq L */
	RegWriteA_LC898122AF(WC_SINFRQ1,
			     (unsigned char)(UsFRQ[UcMeasMode] >> 8));

	/* Clear Optional Sine wave input address */
	RegReadA_LC898122AF(WH_EQSWX, &UcEqSwX); /* 0x0170       */
	RegReadA_LC898122AF(WH_EQSWY, &UcEqSwY); /* 0x0171       */
	if (!UcMeasMode && !UcJikuSel) {	 /* Loop Gain mode  X-axis */
		UcEqSwX |= 0x10;		 /* SW[4] */
		UcEqSwY &= ~EQSINSW;
	} else if (!UcMeasMode && UcJikuSel) { /* Loop Gain mode Y-Axis */
		UcEqSwX &= ~EQSINSW;
		UcEqSwY |= 0x10;	       /* SW[4] */
	} else if (UcMeasMode && !UcJikuSel) { /* Bias/Offset mode X-Axis */
		UcEqSwX = 0x22;		       /* SW[5] */
		UcEqSwY = 0x03;
	} else { /* Bias/Offset mode Y-Axis */
		UcEqSwX = 0x03;
		UcEqSwY = 0x22; /* SW[5] */
	}
	RegWriteA_LC898122AF(WH_EQSWX, UcEqSwX); /* 0x0170       */
	RegWriteA_LC898122AF(WH_EQSWY, UcEqSwY); /* 0x0171       */
}


void StartSineWave(void)
{
	/* Start Sine Wave */
	RegWriteA_LC898122AF(WC_SINON, 0x01); /* 0x0180       Sine wave  */
}


void StopSineWave(void)
{
	unsigned char UcEqSwX, UcEqSwY;

	RegWriteA_LC898122AF(WC_SINON, 0x00); /* 0x0180       Sine wave Stop */
	RegReadA_LC898122AF(WH_EQSWX, &UcEqSwX); /* 0x0170       */
	RegReadA_LC898122AF(WH_EQSWY, &UcEqSwY); /* 0x0171       */
	UcEqSwX &= ~EQSINSW;
	UcEqSwY &= ~EQSINSW;
	RegWriteA_LC898122AF(WH_EQSWX,
			     UcEqSwX); /* 0x0170       Switch control */
	RegWriteA_LC898122AF(WH_EQSWY,
			     UcEqSwY); /* 0x0171       Switch control */
}

void SetMeasFil(unsigned char UcFilSel)
{

	MesFil(UcFilSel); /* Set Measure filter */
}

void ClrMeasFil(void)
{
	/* Measure Filters clear */
	ClrGyr(0x1000, CLR_FRAM1); /* MEAS-FIL Delay RAM Clear */
}

#ifdef MODULE_CALIBRATION

unsigned char LoopGainAdj(unsigned char UcJikuSel)
{

	unsigned short UsRltVal;
	unsigned char UcAdjSts = FAILURE;

	UcJikuSel &= 0x01;

	StbOnn(); /* Slope Mode */

	/* Wait 200ms */
	WitTim_LC898122AF(200);

	/* set start gain */
	LopPar(UcJikuSel);

	/* set sine wave */
	SetSineWave(UcJikuSel, __MEASURE_LOOPGAIN);

	/* Measure count */
	RegWriteA_LC898122AF(WC_MESLOOP1, 0x00); /* 0x0193 */
	RegWriteA_LC898122AF(WC_MESLOOP0,
			     0x40); /* 0x0192       64 Times Measure */
	RamWrite32A_LC898122AF(msmean,
			       0x3C800000); /* 0x1230       1/CmMesLoop[15:0] */
	RegWriteA_LC898122AF(WC_MESABS, 0x01); /* 0x0198       ABS */

	/* Set Adjustment Limits */
	RamWrite32A_LC898122AF(
		LGMax,
		0x3F800000); /* 0x1092       Loop gain adjustment max limit */
	RamWrite32A_LC898122AF(
		LGMin,
		0x3E000100); /* 0x1091       Loop gain adjustment min limit */
	RegWriteA_LC898122AF(WC_AMJLOOP1,
			     0x00); /* 0x01A3       Time-Out time */
	RegWriteA_LC898122AF(WC_AMJLOOP0,
			     0x41);		/* 0x01A2       Time-Out time */
	RegWriteA_LC898122AF(WC_AMJIDL1, 0x00); /* 0x01A5       wait */
	RegWriteA_LC898122AF(WC_AMJIDL0, 0x00); /* 0x01A4       wait */

	/* set Measure Filter */
	SetMeasFil(LOOPGAIN);

	/* Clear Measure Filters */
	ClrMeasFil();

	/* Start Sine Wave */
	StartSineWave();

	/* Enable Loop Gain Adjustment */
	/* Check Busy Flag */
	BsyWit(WC_AMJMODE,
	       (0x0E |
		(UcJikuSel << 4))); /* 0x01A0       Loop Gain adjustment */

	RegReadA_LC898122AF(RC_AMJERROR, &UcAdjBsy); /* 0x01AD */

	/* Ram Access */
	RamAccFixMod(ON); /* Fix mode */

	if (UcAdjBsy) {
		if (UcJikuSel == X_DIR) {
			RamReadA_LC898122AF(sxg, &UsRltVal); /* 0x10D3 */
			StAdjPar.StLopGan.UsLxgVal = UsRltVal;
			StAdjPar.StLopGan.UsLxgSts = 0x0000;
		} else {
			RamReadA_LC898122AF(syg, &UsRltVal); /* 0x11D3 */
			StAdjPar.StLopGan.UsLygVal = UsRltVal;
			StAdjPar.StLopGan.UsLygSts = 0x0000;
		}

	} else {
		if (UcJikuSel == X_DIR) {
			RamReadA_LC898122AF(sxg, &UsRltVal); /* 0x10D3 */
			StAdjPar.StLopGan.UsLxgVal = UsRltVal;
			StAdjPar.StLopGan.UsLxgSts = 0xFFFF;
		} else {
			RamReadA_LC898122AF(syg, &UsRltVal); /* 0x11D3 */
			StAdjPar.StLopGan.UsLygVal = UsRltVal;
			StAdjPar.StLopGan.UsLygSts = 0xFFFF;
		}
		UcAdjSts = SUCCESS; /* Status OK */
	}

	/* Ram Access */
	RamAccFixMod(OFF); /* Float mode */

	/* Stop Sine Wave */
	StopSineWave();

	return UcAdjSts;
}
#endif

unsigned char BiasOffsetAdj(unsigned char UcJikuSel, unsigned char UcMeasCnt)
{
	unsigned char UcHadjRst;

	unsigned long UlTgtVal[2][5] = {{0x3F800000, 0x3D200140, 0xBD200140,
					 0x3F547AE1, 0x3F451EB8}, /* ROUGH */
					{0x3F000000, 0x3D200140, 0xBD200140,
					 0x3F50A3D7, 0x3F48F5C3} }; /* FINE */

	if (UcMeasCnt > 1)
		UcMeasCnt = 1;

	UcJikuSel &= 0x01;

	/* Set Sine Wave */
	SetSineWave(UcJikuSel, __MEASURE_BIASOFFSET);

	/* Measure count */
	RegWriteA_LC898122AF(WC_MESLOOP1, 0x00); /* 0x0193 */
	RegWriteA_LC898122AF(WC_MESLOOP0,
			     0x04); /* 0x0192       4 Times Measure */
	RamWrite32A_LC898122AF(
		msmean, 0x3E000000); /* 0x10AE       1/CmMesLoop[15:0]/2 */
	RegWriteA_LC898122AF(WC_MESABS, 0x00); /* 0x0198       ABS */

	/* Set Adjustment Limits */
	RamWrite32A_LC898122AF(
		HOStp,
		UlTgtVal[UcMeasCnt][0]); /* 0x1070       Hall Offset Stp */
	RamWrite32A_LC898122AF(
		HOMax, UlTgtVal[UcMeasCnt]
			       [1]); /* 0x1072       Hall Offset max limit */
	RamWrite32A_LC898122AF(
		HOMin, UlTgtVal[UcMeasCnt]
			       [2]); /* 0x1071       Hall Offset min limit */
	RamWrite32A_LC898122AF(
		HBStp, UlTgtVal[UcMeasCnt][0]); /* 0x1080       Hall Bias Stp */
	RamWrite32A_LC898122AF(
		HBMax,
		UlTgtVal[UcMeasCnt][3]); /* 0x1082       Hall Bias max limit */
	RamWrite32A_LC898122AF(
		HBMin,
		UlTgtVal[UcMeasCnt][4]); /* 0x1081       Hall Bias min limit */

	RegWriteA_LC898122AF(WC_AMJLOOP1,
			     0x00); /* 0x01A3       Time-Out time */
	RegWriteA_LC898122AF(WC_AMJLOOP0,
			     0x40);		/* 0x01A2       Time-Out time */
	RegWriteA_LC898122AF(WC_AMJIDL1, 0x00); /* 0x01A5       wait */
	RegWriteA_LC898122AF(WC_AMJIDL0, 0x00); /* 0x01A4       wait */

	/* Set Measure Filter */
	SetMeasFil(HALL_ADJ);

	/* Clear Measure Filters */
	ClrMeasFil();

	/* Start Sine Wave */
	StartSineWave();

	/* Check Busy Flag */
	BsyWit(
		WC_AMJMODE,
		(0x0C |
		 (UcJikuSel
		  << 4))); /* 0x01A0       Hall bais/offset ppt adjustment */

	RegReadA_LC898122AF(RC_AMJERROR, &UcAdjBsy); /* 0x01AD */

	if (UcAdjBsy) {
		if (UcJikuSel == X_DIR)
			UcHadjRst = EXE_HXADJ;
		else
			UcHadjRst = EXE_HYADJ;
	} else {
		UcHadjRst = EXE_END;
	}

	/* Stop Sine Wave */
	StopSineWave();

	/* Set Servo Filter */

	/* Ram Access */
	RamAccFixMod(ON); /* Fix mode */

	if (UcJikuSel == X_DIR) {
		RamReadA_LC898122AF(
			MSPP1AV,
			&StAdjPar.StHalAdj
				 .UsHlxMxa); /* 0x1042 Max width value */
		RamReadA_LC898122AF(
			MSCT1AV,
			&StAdjPar.StHalAdj.UsHlxCna); /* 0x1052 offset value */
	} else {
		RamReadA_LC898122AF(
			MSPP1AV,
			&StAdjPar.StHalAdj
				 .UsHlyMxa); /* 0x1042 Max width value */
		RamReadA_LC898122AF(
			MSCT1AV,
			&StAdjPar.StHalAdj.UsHlyCna); /* 0x1052 offset value */
	}

	StAdjPar.StHalAdj.UsHlxCna =
		(unsigned short)((signed short)StAdjPar.StHalAdj.UsHlxCna << 1);
	StAdjPar.StHalAdj.UsHlyCna =
		(unsigned short)((signed short)StAdjPar.StHalAdj.UsHlyCna << 1);
	/* Ram Access */
	RamAccFixMod(OFF); /* Float mode */

	return UcHadjRst;
}

#endif

void GyrGan(unsigned char UcGygmode, unsigned long UlGygXval,
	    unsigned long UlGygYval)
{
	switch (UcGygmode) {
	case VAL_SET:
		RamWrite32A_LC898122AF(gxzoom, UlGygXval); /* 0x1020 */
		RamWrite32A_LC898122AF(gyzoom, UlGygYval); /* 0x1120 */
		break;
	case VAL_FIX:
		RamWrite32A_LC898122AF(gxzoom, UlGygXval); /* 0x1020 */
		RamWrite32A_LC898122AF(gyzoom, UlGygYval); /* 0x1120 */

		break;
	case VAL_SPC:
		RamRead32A_LC898122AF(gxzoom, &UlGygXval); /* 0x1020 */
		RamRead32A_LC898122AF(gyzoom, &UlGygYval); /* 0x1120 */

		break;
	}
}

void SetPanTiltMode(unsigned char UcPnTmod)
{
	switch (UcPnTmod) {
	case OFF:
		RegWriteA_LC898122AF(WG_PANON, 0x00);
		break;
	case ON:
		RegWriteA_LC898122AF(WG_PANON, 0x01);
		break;
	}
}

#ifdef GAIN_CONT

unsigned char TriSts(void)
{
	unsigned char UcRsltSts = 0;
	unsigned char UcVal;

	RegReadA_LC898122AF(WG_ADJGANGXATO, &UcVal); /* 0x0129 */
	if (UcVal & 0x03) {			     /* Gain control enable? */
		RegReadA_LC898122AF(RG_LEVJUGE, &UcVal); /* 0x01F4 */
		UcRsltSts = UcVal & 0x11;		 /* bit0, bit4 set */
		UcRsltSts |= 0x80;			 /* bit7 ON */
	}
	return UcRsltSts;
}
#endif


unsigned char DrvPwmSw(unsigned char UcSelPwmMod)
{

	switch (UcSelPwmMod) {
	case Mlnp:
		RegWriteA_LC898122AF(DRVFC, 0xF0);
		UcPwmMod = PWMMOD_CVL;
		break;

	case Mpwm:
#ifdef PWM_BREAK
		RegWriteA_LC898122AF(DRVFC, 0x00);
#else
		RegWriteA_LC898122AF(DRVFC, 0xC0);
#endif
		UcPwmMod = PWMMOD_PWM;
		break;
	}

	return UcSelPwmMod << 4;
}

#ifdef NEUTRAL_CENTER

unsigned char TneHvc(void)
{
	unsigned char UcRsltSts;
	unsigned short UsMesRlt1;
	unsigned short UsMesRlt2;

	SrvCon(X_DIR, OFF); /* X Servo OFF */
	SrvCon(Y_DIR, OFF); /* Y Servo OFF */

	WitTim_LC898122AF(500);

	/* 平均値測定 */

	MesFil(THROUGH); /* Set Measure Filter */

	RegWriteA_LC898122AF(WC_MESLOOP1,
			     0x00); /* 0x0193       CmMesLoop[15:8] */
	RegWriteA_LC898122AF(WC_MESLOOP0,
			     0x40); /* 0x0192       CmMesLoop[7:0] */

	RamWrite32A_LC898122AF(msmean,
			       0x3C800000); /* 0x1230       1/CmMesLoop[15:0] */

	RegWriteA_LC898122AF(WC_MES1ADD0, (unsigned char)AD0Z); /* 0x0194 */
	RegWriteA_LC898122AF(WC_MES1ADD1, (unsigned char)((AD0Z >> 8) &
							  0x0001)); /* 0x0195 */
	RegWriteA_LC898122AF(WC_MES2ADD0, (unsigned char)AD1Z); /* 0x0196 */
	RegWriteA_LC898122AF(WC_MES2ADD1, (unsigned char)((AD1Z >> 8) &
							  0x0001)); /* 0x0197 */

	RamWrite32A_LC898122AF(MSABS1AV, 0x00000000); /* 0x1041 */
	RamWrite32A_LC898122AF(MSABS2AV, 0x00000000); /* 0x1141 */

	RegWriteA_LC898122AF(WC_MESABS, 0x00); /* 0x0198       none ABS */

	BsyWit(WC_MESMODE, 0x01); /* 0x0190               Normal Measure */

	RamAccFixMod(ON); /* Fix mode */

	RamReadA_LC898122AF(MSABS1AV,
			    &UsMesRlt1); /* 0x1041       Measure Result */
	RamReadA_LC898122AF(MSABS2AV,
			    &UsMesRlt2); /* 0x1141       Measure Result */

	RamAccFixMod(OFF); /* Float mode */

	StAdjPar.StHalAdj.UsHlxCna = UsMesRlt1; /* Measure Result Store */
	StAdjPar.StHalAdj.UsHlxCen = UsMesRlt1; /* Measure Result Store */

	StAdjPar.StHalAdj.UsHlyCna = UsMesRlt2; /* Measure Result Store */
	StAdjPar.StHalAdj.UsHlyCen = UsMesRlt2; /* Measure Result Store */

	UcRsltSts = EXE_END; /* Clear Status */

	return UcRsltSts;
}
#endif /* NEUTRAL_CENTER */

void SetGcf(unsigned char UcSetNum)
{

	/* Zoom Step */
	if (UcSetNum > (COEFTBL - 1))
		UcSetNum = (COEFTBL - 1); /* 上限をCOEFTBL-1に設定する */

	UlH1Coefval = ClDiCof[UcSetNum];

	/* Zoom Value Setting */
	RamWrite32A_LC898122AF(gxh1c, UlH1Coefval); /* 0x1012 */
	RamWrite32A_LC898122AF(gyh1c, UlH1Coefval); /* 0x1112 */

#ifdef H1COEF_CHANGER
	SetH1cMod(UcSetNum); /* Re-setting */
#endif
}

#ifdef H1COEF_CHANGER

void SetH1cMod(unsigned char UcSetNum)
{

	switch (UcSetNum) {
	case (ACTMODE):		  /* initial */
		IniPtMovMod(OFF); /* Pan/Tilt setting (Still) */

		/* enable setting */
		/* Zoom Step */
		UlH1Coefval = ClDiCof[0];

		UcH1LvlMod = 0;

		/* Limit value Value Setting */
		RamWrite32A_LC898122AF(gxlmt6L, MINLMT); /* 0x102D L-Limit */
		RamWrite32A_LC898122AF(gxlmt6H, MAXLMT); /* 0x102E H-Limit */

		RamWrite32A_LC898122AF(gylmt6L, MINLMT); /* 0x112D L-Limit */
		RamWrite32A_LC898122AF(gylmt6H, MAXLMT); /* 0x112E H-Limit */

		RamWrite32A_LC898122AF(gxhc_tmp,
				       UlH1Coefval); /* 0x100E Base Coef */
		RamWrite32A_LC898122AF(
			gxmg, CHGCOEF); /* 0x10AA Change coefficient gain */

		RamWrite32A_LC898122AF(gyhc_tmp,
				       UlH1Coefval); /* 0x110E Base Coef */
		RamWrite32A_LC898122AF(
			gymg, CHGCOEF); /* 0x11AA Change coefficient gain */

		RegWriteA_LC898122AF(
			WG_HCHR, 0x12); /* 0x011B       GmHChrOn[1]=1 Sw ON */
		break;

	case (S2MODE): /* cancel lvl change mode */
		RegWriteA_LC898122AF(
			WG_HCHR, 0x10); /* 0x011B       GmHChrOn[1]=0 Sw OFF */
		break;

	case (MOVMODE):		 /* Movie mode */
		IniPtMovMod(ON); /* Pan/Tilt setting (Movie) */

		RamWrite32A_LC898122AF(gxlmt6L,
				       MINLMT_MOV); /* 0x102D L-Limit */
		RamWrite32A_LC898122AF(gylmt6L,
				       MINLMT_MOV); /* 0x112D L-Limit */

		RamWrite32A_LC898122AF(
			gxmg, CHGCOEF_MOV); /* 0x10AA Change coefficient gain */
		RamWrite32A_LC898122AF(
			gymg, CHGCOEF_MOV); /* 0x11AA Change coefficient gain */

		RamWrite32A_LC898122AF(gxhc_tmp,
				       UlH1Coefval); /* 0x100E Base Coef */
		RamWrite32A_LC898122AF(gyhc_tmp,
				       UlH1Coefval); /* 0x110E Base Coef */

		RegWriteA_LC898122AF(
			WG_HCHR, 0x12); /* 0x011B       GmHChrOn[1]=1 Sw ON */
		break;

	default:
		IniPtMovMod(OFF); /* Pan/Tilt setting (Still) */

		UcH1LvlMod = UcSetNum;

		RamWrite32A_LC898122AF(gxlmt6L, MINLMT); /* 0x102D L-Limit */
		RamWrite32A_LC898122AF(gylmt6L, MINLMT); /* 0x112D L-Limit */

		RamWrite32A_LC898122AF(
			gxmg, CHGCOEF); /* 0x10AA Change coefficient gain */
		RamWrite32A_LC898122AF(
			gymg, CHGCOEF); /* 0x11AA Change coefficient gain */

		RamWrite32A_LC898122AF(gxhc_tmp,
				       UlH1Coefval); /* 0x100E Base Coef */
		RamWrite32A_LC898122AF(gyhc_tmp,
				       UlH1Coefval); /* 0x110E Base Coef */

		RegWriteA_LC898122AF(
			WG_HCHR, 0x12); /* 0x011B       GmHChrOn[1]=1 Sw ON */
		break;
	}
}
#endif

unsigned short RdFwVr(void)
{
	unsigned short UsVerVal;

	UsVerVal = (unsigned short)((MDL_VER << 8) | FW_VER);
	return UsVerVal;
}
